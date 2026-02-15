//! Windows-specific process implementation using named pipes and overlapped I/O.

use std::ffi::CString;
use std::mem::{self, zeroed};
use std::path::Path;
use std::ptr::{addr_of_mut, null, null_mut};
use std::sync::atomic::{AtomicU32, Ordering};
use std::time::{Duration, Instant};

use windows_sys::Win32::{
    Foundation::{
        CloseHandle, GetLastError, SetHandleInformation, HANDLE, HANDLE_FLAG_INHERIT,
        INVALID_HANDLE_VALUE, WAIT_OBJECT_0, WAIT_TIMEOUT,
    },
    Security::SECURITY_ATTRIBUTES,
    Storage::FileSystem::{
        CreateFileA, ReadFile, WriteFile, FILE_ATTRIBUTE_NORMAL, FILE_FLAG_OVERLAPPED,
        FILE_SHARE_MODE, OPEN_EXISTING,
    },
    System::{
        Pipes::CreateNamedPipeA,
        Threading::{
            CreateEventA, CreateProcessA, GetCurrentProcessId, GetExitCodeProcess,
            TerminateProcess, WaitForSingleObject, PROCESS_INFORMATION, STARTF_USESTDHANDLES,
            STARTUPINFOA,
        },
        IO::{CancelIo, GetOverlappedResult, OVERLAPPED},
    },
};

use super::common::{Line, ProcessError, ProcessResult, Standard};

const BUFFER_SIZE: usize = 4096;
const PIPE_ACCESS_INBOUND: u32 = 0x0000_0001;
const PIPE_TYPE_BYTE: u32 = 0x0000_0000;
const PIPE_WAIT: u32 = 0x0000_0000;
const GENERIC_WRITE: u32 = 0x4000_0000;
const STILL_ACTIVE: u32 = 259;
const ERROR_IO_PENDING: u32 = 997;
const CREATE_NEW_PROCESS_GROUP: u32 = 0x0000_0200;
const PIPE_TIMEOUT_MS: u32 = 120_000;

static PIPE_SERIAL: AtomicU32 = AtomicU32::new(0);

/// RAII wrapper for Windows HANDLEs.
struct SafeHandle(HANDLE);

impl SafeHandle {
    fn new(h: HANDLE) -> Option<Self> {
        if h == INVALID_HANDLE_VALUE || h.is_null() {
            None
        } else {
            Some(Self(h))
        }
    }

    fn raw(&self) -> HANDLE {
        self.0
    }

    /// Takes ownership out, returning the raw handle and preventing Drop from closing it.
    fn take(mut self) -> HANDLE {
        let h = self.0;
        self.0 = INVALID_HANDLE_VALUE;
        h
    }
}

impl Drop for SafeHandle {
    fn drop(&mut self) {
        if self.0 != INVALID_HANDLE_VALUE && !self.0.is_null() {
            unsafe { CloseHandle(self.0) };
        }
    }
}

/// A named pipe pair (read end, write end).
struct PipePair {
    read: SafeHandle,
    write: SafeHandle,
}

fn create_pipe_pair(sa: &mut SECURITY_ATTRIBUTES) -> Result<PipePair, ProcessError> {
    let pid = unsafe { GetCurrentProcessId() };
    let serial = PIPE_SERIAL.fetch_add(1, Ordering::Relaxed);
    let name = CString::new(format!("\\\\.\\Pipe\\RemoteExeAnon.{pid:08x}.{serial:08x}"))
        .map_err(|_| ProcessError::from("Invalid pipe name"))?;

    let read = unsafe {
        CreateNamedPipeA(
            name.as_ptr() as *const u8,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,
            BUFFER_SIZE as u32,
            BUFFER_SIZE as u32,
            PIPE_TIMEOUT_MS,
            sa as *mut _,
        )
    };
    let read = SafeHandle::new(read).ok_or_else(|| ProcessError::from("CreateNamedPipe failed"))?;

    let write = unsafe {
        CreateFileA(
            name.as_ptr() as *const u8,
            GENERIC_WRITE,
            0 as FILE_SHARE_MODE,
            sa as *mut _,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            INVALID_HANDLE_VALUE,
        )
    };
    let write =
        SafeHandle::new(write).ok_or_else(|| ProcessError::from("CreateFile for pipe failed"))?;

    Ok(PipePair { read, write })
}

fn win_err(msg: &str) -> ProcessError {
    let code = unsafe { GetLastError() };
    ProcessError::from(format!("{msg}: error {code}"))
}

fn set_no_inherit(handle: HANDLE) -> Result<(), ProcessError> {
    if unsafe { SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0) } == 0 {
        Err(win_err("SetHandleInformation failed"))
    } else {
        Ok(())
    }
}

pub struct Process {
    h_process: HANDLE,
    h_thread: HANDLE,
    process_id: u32,
    h_stdout_read: HANDLE,
    h_stdin_write: HANDLE,
    realtime_logging: bool,
    log_name: String,
    line_buffer_out: String,
    initialized: bool,
}

unsafe impl Send for Process {}

impl Process {
    pub fn new() -> Result<Self, ProcessResult> {
        Ok(Self {
            h_process: INVALID_HANDLE_VALUE,
            h_thread: INVALID_HANDLE_VALUE,
            process_id: 0,
            h_stdout_read: INVALID_HANDLE_VALUE,
            h_stdin_write: INVALID_HANDLE_VALUE,
            realtime_logging: true,
            log_name: String::new(),
            line_buffer_out: String::with_capacity(300),
            initialized: false,
        })
    }

    pub fn set_realtime_logging(&mut self, rt: bool) {
        self.realtime_logging = rt;
    }

    pub fn init(
        &mut self,
        working_dir: &str,
        command: &str,
        args: &str,
        log_name: &str,
    ) -> ProcessResult {
        self.log_name = log_name.to_string();

        let mut sa = SECURITY_ATTRIBUTES {
            nLength: mem::size_of::<SECURITY_ATTRIBUTES>() as u32,
            lpSecurityDescriptor: null_mut(),
            bInheritHandle: 1,
        };

        let stdout_pipe = match create_pipe_pair(&mut sa) {
            Ok(p) => p,
            Err(e) => return ProcessResult::error(e.to_string()),
        };
        let stdin_pipe = match create_pipe_pair(&mut sa) {
            Ok(p) => p,
            Err(e) => return ProcessResult::error(e.to_string()),
        };

        // Prevent child from inheriting the ends we keep.
        if let Err(e) = set_no_inherit(stdout_pipe.read.raw()) {
            return ProcessResult::error(e.to_string());
        }
        if let Err(e) = set_no_inherit(stdin_pipe.write.raw()) {
            return ProcessResult::error(e.to_string());
        }

        let full_path = Path::new(working_dir)
            .join(command)
            .to_string_lossy()
            .replace('/', "\\");

        let cmd_line = if args.is_empty() {
            full_path
        } else {
            format!("{full_path} {args}")
        };

        let cmd_cstring = match CString::new(cmd_line) {
            Ok(c) => c,
            Err(_) => return ProcessResult::error("Invalid command string"),
        };

        let mut si: STARTUPINFOA = unsafe { zeroed() };
        si.cb = mem::size_of::<STARTUPINFOA>() as u32;
        si.hStdOutput = stdout_pipe.write.raw();
        si.hStdInput = stdin_pipe.read.raw();
        si.hStdError = stdout_pipe.write.raw();
        si.dwFlags = STARTF_USESTDHANDLES;

        let wd_cstring = if working_dir.is_empty() {
            None
        } else {
            CString::new(working_dir).ok()
        };
        let wd_ptr = wd_cstring
            .as_ref()
            .map_or(null(), |s| s.as_ptr() as *const u8);

        let mut pi: PROCESS_INFORMATION = unsafe { zeroed() };

        let success = unsafe {
            CreateProcessA(
                null_mut(),
                cmd_cstring.as_ptr() as *mut u8,
                null_mut(),
                null_mut(),
                1, // inherit handles
                CREATE_NEW_PROCESS_GROUP,
                null_mut(),
                wd_ptr as *mut u8,
                addr_of_mut!(si),
                addr_of_mut!(pi),
            )
        };

        if success == 0 {
            return ProcessResult::error(format!("Failed to create process: {}", unsafe {
                GetLastError()
            }));
        }

        self.h_process = pi.hProcess;
        self.h_thread = pi.hThread;
        self.process_id = pi.dwProcessId;

        // Keep only our ends; child ends are dropped here via SafeHandle.
        self.h_stdout_read = stdout_pipe.read.take();
        self.h_stdin_write = stdin_pipe.write.take();
        // stdout_pipe.write and stdin_pipe.read drop here, closing the child's ends.

        self.initialized = true;
        ProcessResult::ok()
    }

    pub fn alive(&mut self) -> ProcessResult {
        if self.h_process == INVALID_HANDLE_VALUE {
            return ProcessResult::error("Process not initialized");
        }

        let mut exit_code: u32 = 0;
        if unsafe { GetExitCodeProcess(self.h_process, &mut exit_code) } == 0 {
            return ProcessResult::error("Failed to get exit code");
        }

        if exit_code == STILL_ACTIVE {
            ProcessResult::ok()
        } else {
            ProcessResult::error(format!("Process exited with code: {exit_code}"))
        }
    }

    pub fn setup_read(&mut self) {
        self.line_buffer_out.clear();
    }

    pub fn read_output(
        &mut self,
        lines: &mut Vec<Line>,
        searchword: &str,
        timeout: Duration,
    ) -> ProcessResult {
        lines.clear();

        if self.h_stdout_read == INVALID_HANDLE_VALUE {
            return ProcessResult::error("Process not initialized");
        }

        let start = Instant::now();
        let mut buffer = [0u8; BUFFER_SIZE];

        loop {
            let elapsed = start.elapsed();
            if !timeout.is_zero() && elapsed >= timeout {
                return ProcessResult::timeout();
            }

            let event = match SafeHandle::new(unsafe { CreateEventA(null_mut(), 1, 0, null()) }) {
                Some(e) => e,
                None => return ProcessResult::error("Failed to create event"),
            };

            let mut overlapped: OVERLAPPED = unsafe { zeroed() };
            overlapped.hEvent = event.raw();

            let mut bytes_read: u32 = 0;
            let result = unsafe {
                ReadFile(
                    self.h_stdout_read,
                    buffer.as_mut_ptr(),
                    buffer.len() as u32,
                    &mut bytes_read,
                    &mut overlapped,
                )
            };

            if result == 0 {
                let error = unsafe { GetLastError() };
                if error != ERROR_IO_PENDING {
                    return ProcessResult::error(format!("Read failed: {error}"));
                }

                let remaining_ms = if timeout.is_zero() {
                    u32::MAX
                } else {
                    timeout.saturating_sub(elapsed).as_millis() as u32
                };

                let wait = unsafe { WaitForSingleObject(event.raw(), remaining_ms) };

                if wait == WAIT_TIMEOUT {
                    unsafe { CancelIo(self.h_stdout_read) };
                    return ProcessResult::timeout();
                }
                if wait != WAIT_OBJECT_0 {
                    return ProcessResult::error("Wait failed");
                }

                if unsafe {
                    GetOverlappedResult(self.h_stdout_read, &mut overlapped, &mut bytes_read, 0)
                } == 0
                {
                    return ProcessResult::error("Failed to get overlapped result");
                }
            }
            // event dropped here automatically

            if bytes_read == 0 {
                continue;
            }

            for &byte in &buffer[..bytes_read as usize] {
                let c = byte as char;
                if c == '\n' || c == '\r' {
                    if self.line_buffer_out.is_empty() {
                        continue;
                    }

                    let ts = crate::core::datetime_precise();

                    if self.realtime_logging {
                        crate::core::logger::LOGGER.read_from_engine(
                            &self.line_buffer_out,
                            &ts,
                            &self.log_name,
                            false,
                            None,
                        );
                    }

                    lines.push(Line {
                        line: self.line_buffer_out.clone(),
                        time: ts,
                        std: Standard::Output,
                    });

                    let found =
                        !searchword.is_empty() && self.line_buffer_out.starts_with(searchword);

                    self.line_buffer_out.clear();

                    if found {
                        return ProcessResult::ok();
                    }
                } else {
                    self.line_buffer_out.push(c);
                }
            }
        }
    }

    pub fn write_input(&mut self, input: &str) -> ProcessResult {
        if self.h_stdin_write == INVALID_HANDLE_VALUE {
            return ProcessResult::error("Process not initialized");
        }

        let mut bytes_written: u32 = 0;
        let result = unsafe {
            WriteFile(
                self.h_stdin_write,
                input.as_ptr(),
                input.len() as u32,
                &mut bytes_written,
                null_mut(),
            )
        };

        if result == 0 {
            ProcessResult::error("Failed to write to stdin")
        } else {
            ProcessResult::ok()
        }
    }

    pub fn set_affinity(&self, _cpus: &[i32]) -> bool {
        false
    }

    pub fn interrupt(&self) {}

    pub fn pid(&self) -> Option<u32> {
        (self.process_id != 0).then_some(self.process_id)
    }

    fn close_handle(h: &mut HANDLE) {
        if *h != INVALID_HANDLE_VALUE {
            unsafe { CloseHandle(*h) };
            *h = INVALID_HANDLE_VALUE;
        }
    }
}

impl Drop for Process {
    fn drop(&mut self) {
        Self::close_handle(&mut self.h_stdout_read);
        Self::close_handle(&mut self.h_stdin_write);

        if self.h_process == INVALID_HANDLE_VALUE || !self.initialized {
            return;
        }

        let deadline = Instant::now() + Duration::from_millis(100);

        loop {
            let mut exit_code: u32 = 0;
            unsafe { GetExitCodeProcess(self.h_process, &mut exit_code) };

            if exit_code != STILL_ACTIVE {
                break;
            }
            if Instant::now() >= deadline {
                unsafe { TerminateProcess(self.h_process, 1) };
                break;
            }
            std::thread::sleep(Duration::from_millis(10));
        }

        Self::close_handle(&mut self.h_process);
        Self::close_handle(&mut self.h_thread);
    }
}
