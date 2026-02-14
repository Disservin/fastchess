//! Windows-specific process implementation using named pipes and overlapped I/O.

use std::mem::zeroed;
use std::path::Path;
use std::ptr::{addr_of_mut, null, null_mut};
use std::time::Duration;

use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, HANDLE, INVALID_HANDLE_VALUE, WAIT_OBJECT_0, WAIT_TIMEOUT,
};
use windows_sys::Win32::Foundation::{SetHandleInformation, HANDLE_FLAG_INHERIT};
use windows_sys::Win32::Security::SECURITY_ATTRIBUTES;
use windows_sys::Win32::Storage::FileSystem::{
    CreateFileA, ReadFile, WriteFile, FILE_ATTRIBUTE_NORMAL, FILE_FLAG_OVERLAPPED, FILE_SHARE_MODE,
    OPEN_EXISTING,
};
use windows_sys::Win32::System::Pipes::CreateNamedPipeA;
use windows_sys::Win32::System::Threading::{
    CreateEventA, CreateProcessA, GetExitCodeProcess, TerminateProcess, WaitForSingleObject,
    PROCESS_INFORMATION, STARTF_USESTDHANDLES, STARTUPINFOA,
};
use windows_sys::Win32::System::IO::{CancelIo, GetOverlappedResult, OVERLAPPED};

use super::common::{Line, ProcessError, ProcessResult, Standard};

const BUFFER_SIZE: usize = 4096;
const PIPE_ACCESS_INBOUND: u32 = 0x00000001;
const PIPE_TYPE_BYTE: u32 = 0x00000000;
const PIPE_WAIT: u32 = 0x00000000;
const GENERIC_WRITE: u32 = 0x40000000;
const STILL_ACTIVE: u32 = 259;
const CREATE_NEW_PROCESS_GROUP: u32 = 0x00000200;

pub struct Process {
    h_process: HANDLE,
    h_thread: HANDLE,
    process_id: u32,
    h_stdout_read: HANDLE,
    h_stdout_write: HANDLE,
    h_stdin_read: HANDLE,
    h_stdin_write: HANDLE,
    realtime_logging: bool,
    log_name: String,
    line_buffer_out: String,
    initialized: bool,
}

unsafe impl Send for Process {}

impl Default for Process {
    fn default() -> Self {
        Self::new()
    }
}

impl Process {
    pub fn new() -> Result<Self, ProcessResult> {
        Ok(Self {
            h_process: INVALID_HANDLE_VALUE,
            h_thread: INVALID_HANDLE_VALUE,
            process_id: 0,
            h_stdout_read: INVALID_HANDLE_VALUE,
            h_stdout_write: INVALID_HANDLE_VALUE,
            h_stdin_read: INVALID_HANDLE_VALUE,
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

        if !self.create_pipes() {
            self.close_handles();
            return ProcessResult::error("Failed to create pipes");
        }

        unsafe {
            if SetHandleInformation(self.h_stdout_read, HANDLE_FLAG_INHERIT, 0) == 0 {
                self.close_handles();
                return ProcessResult::error("Failed to set stdout handle information");
            }
            if SetHandleInformation(self.h_stdin_write, HANDLE_FLAG_INHERIT, 0) == 0 {
                self.close_handles();
                return ProcessResult::error("Failed to set stdin handle information");
            }
        }

        let full_path = Path::new(working_dir).join(command);
        let cmd_line = if args.is_empty() {
            format!("{}", full_path.to_string_lossy().replace("/", "\\"))
        } else {
            format!(
                "{} {}",
                full_path.to_string_lossy().replace("/", "\\"),
                args
            )
        };

        let mut cmd_cstring = std::ffi::CString::new(cmd_line).unwrap();

        let mut si: STARTUPINFOA = unsafe { zeroed() };
        si.cb = std::mem::size_of::<STARTUPINFOA>() as u32;
        si.hStdOutput = self.h_stdout_write;
        si.hStdInput = self.h_stdin_read;
        si.hStdError = self.h_stdout_write;
        si.dwFlags = STARTF_USESTDHANDLES;

        let wd_cstring = if working_dir.is_empty() {
            None
        } else {
            Some(std::ffi::CString::new(working_dir).unwrap())
        };
        let wd_ptr = wd_cstring
            .as_ref()
            .map_or(null_mut(), |s| s.as_ptr() as *mut u8);

        let mut pi: PROCESS_INFORMATION = unsafe { zeroed() };

        let success = unsafe {
            CreateProcessA(
                null_mut(),
                cmd_cstring.as_ptr() as *mut u8,
                null_mut(),
                null_mut(),
                1,
                CREATE_NEW_PROCESS_GROUP,
                null_mut(),
                wd_ptr,
                addr_of_mut!(si),
                addr_of_mut!(pi),
            )
        };

        if success == 0 {
            self.close_handles();
            return ProcessResult::error(format!("Failed to create process: {}", unsafe {
                GetLastError()
            }));
        }

        self.h_process = pi.hProcess;
        self.h_thread = pi.hThread;
        self.process_id = pi.dwProcessId;

        unsafe {
            CloseHandle(self.h_stdout_write);
            CloseHandle(self.h_stdin_read);
        }
        self.h_stdout_write = INVALID_HANDLE_VALUE;
        self.h_stdin_read = INVALID_HANDLE_VALUE;

        self.initialized = true;

        ProcessResult::ok()
    }

    fn create_pipes(&mut self) -> bool {
        use std::sync::atomic::{AtomicU32, Ordering};
        static PIPE_SERIAL: AtomicU32 = AtomicU32::new(0);

        let pid = unsafe { windows_sys::Win32::System::Threading::GetCurrentProcessId() };
        let serial = PIPE_SERIAL.fetch_add(1, Ordering::SeqCst);

        let mut sa = SECURITY_ATTRIBUTES {
            nLength: std::mem::size_of::<SECURITY_ATTRIBUTES>() as u32,
            lpSecurityDescriptor: null_mut(),
            bInheritHandle: 1,
        };

        // stdout pipe
        let pipe_name_out = format!("\\\\.\\Pipe\\RemoteExeAnon.{:08x}.{:08x}", pid, serial);
        let pipe_name_out_c = std::ffi::CString::new(&*pipe_name_out).unwrap();

        self.h_stdout_read = unsafe {
            CreateNamedPipeA(
                pipe_name_out_c.as_ptr() as *const u8,
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_WAIT,
                1,
                BUFFER_SIZE as u32,
                BUFFER_SIZE as u32,
                120_000,
                addr_of_mut!(sa),
            )
        };

        if self.h_stdout_read == INVALID_HANDLE_VALUE {
            return false;
        }

        self.h_stdout_write = unsafe {
            CreateFileA(
                pipe_name_out_c.as_ptr() as *const u8,
                GENERIC_WRITE,
                0 as FILE_SHARE_MODE,
                addr_of_mut!(sa),
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                INVALID_HANDLE_VALUE,
            )
        };

        if self.h_stdout_write == INVALID_HANDLE_VALUE {
            unsafe { CloseHandle(self.h_stdout_read) };
            self.h_stdout_read = INVALID_HANDLE_VALUE;
            return false;
        }

        // stdin pipe
        let serial_in = PIPE_SERIAL.fetch_add(1, Ordering::SeqCst);
        let pipe_name_in = format!("\\\\.\\Pipe\\RemoteExeAnon.{:08x}.{:08x}", pid, serial_in);
        let pipe_name_in_c = std::ffi::CString::new(&*pipe_name_in).unwrap();

        self.h_stdin_read = unsafe {
            CreateNamedPipeA(
                pipe_name_in_c.as_ptr() as *const u8,
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_WAIT,
                1,
                BUFFER_SIZE as u32,
                BUFFER_SIZE as u32,
                120_000,
                addr_of_mut!(sa),
            )
        };

        if self.h_stdin_read == INVALID_HANDLE_VALUE {
            unsafe {
                CloseHandle(self.h_stdout_read);
                CloseHandle(self.h_stdout_write);
            }
            self.h_stdout_read = INVALID_HANDLE_VALUE;
            self.h_stdout_write = INVALID_HANDLE_VALUE;
            return false;
        }

        self.h_stdin_write = unsafe {
            CreateFileA(
                pipe_name_in_c.as_ptr() as *const u8,
                GENERIC_WRITE,
                0 as FILE_SHARE_MODE,
                addr_of_mut!(sa),
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                INVALID_HANDLE_VALUE,
            )
        };

        if self.h_stdin_write == INVALID_HANDLE_VALUE {
            unsafe {
                CloseHandle(self.h_stdout_read);
                CloseHandle(self.h_stdout_write);
                CloseHandle(self.h_stdin_read);
            }
            self.h_stdout_read = INVALID_HANDLE_VALUE;
            self.h_stdout_write = INVALID_HANDLE_VALUE;
            self.h_stdin_read = INVALID_HANDLE_VALUE;
            return false;
        }

        true
    }

    fn close_handles(&mut self) {
        unsafe {
            if self.h_stdout_read != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_stdout_read);
                self.h_stdout_read = INVALID_HANDLE_VALUE;
            }
            if self.h_stdout_write != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_stdout_write);
                self.h_stdout_write = INVALID_HANDLE_VALUE;
            }
            if self.h_stdin_read != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_stdin_read);
                self.h_stdin_read = INVALID_HANDLE_VALUE;
            }
            if self.h_stdin_write != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_stdin_write);
                self.h_stdin_write = INVALID_HANDLE_VALUE;
            }
        }
    }

    pub fn alive(&mut self) -> ProcessResult {
        if self.h_process == INVALID_HANDLE_VALUE {
            return ProcessResult::error("Process not initialized");
        }

        let mut exit_code: u32 = 0;
        let result = unsafe { GetExitCodeProcess(self.h_process, addr_of_mut!(exit_code)) };

        if result == 0 {
            return ProcessResult::error("Failed to get exit code");
        }

        if exit_code == STILL_ACTIVE {
            ProcessResult::ok()
        } else {
            ProcessResult::error(format!("Process exited with code: {}", exit_code))
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

        let start_time = std::time::Instant::now();
        let timeout_ms = if timeout.is_zero() {
            u32::MAX
        } else {
            timeout.as_millis() as u32
        };

        let mut buffer = [0u8; BUFFER_SIZE];

        loop {
            let elapsed = start_time.elapsed();
            if elapsed >= timeout && !timeout.is_zero() {
                return ProcessResult::timeout();
            }

            let event = unsafe { CreateEventA(null_mut(), 1, 0, null()) };
            if event.is_null() {
                return ProcessResult::error("Failed to create event");
            }

            let mut overlapped: OVERLAPPED = unsafe { zeroed() };
            overlapped.hEvent = event;

            let mut bytes_read: u32 = 0;
            let result = unsafe {
                ReadFile(
                    self.h_stdout_read,
                    buffer.as_mut_ptr(),
                    buffer.len() as u32,
                    addr_of_mut!(bytes_read),
                    addr_of_mut!(overlapped),
                )
            };

            if result == 0 {
                let error = unsafe { GetLastError() };
                if error != 997 {
                    // ERROR_IO_PENDING
                    unsafe { CloseHandle(event) };
                    return ProcessResult::error(format!("Read failed: {}", error));
                }

                let remaining_ms = if timeout.is_zero() {
                    timeout_ms
                } else {
                    let remaining = timeout.saturating_sub(elapsed);
                    remaining.as_millis() as u32
                };

                let wait_result = unsafe { WaitForSingleObject(event, remaining_ms) };

                if wait_result == WAIT_TIMEOUT {
                    unsafe {
                        CancelIo(self.h_stdout_read);
                        CloseHandle(event);
                    }
                    return ProcessResult::timeout();
                }

                if wait_result != WAIT_OBJECT_0 {
                    unsafe { CloseHandle(event) };
                    return ProcessResult::error("Wait failed");
                }

                if unsafe {
                    GetOverlappedResult(
                        self.h_stdout_read,
                        addr_of_mut!(overlapped),
                        addr_of_mut!(bytes_read),
                        0,
                    )
                } == 0
                {
                    unsafe { CloseHandle(event) };
                    return ProcessResult::error("Failed to get overlapped result");
                }
            }

            unsafe { CloseHandle(event) };

            if bytes_read == 0 {
                continue;
            }

            for i in 0..bytes_read as usize {
                let c = buffer[i] as char;
                if c == '\n' || c == '\r' {
                    if !self.line_buffer_out.is_empty() {
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

                        if !searchword.is_empty() && self.line_buffer_out.starts_with(searchword) {
                            self.line_buffer_out.clear();
                            return ProcessResult::ok();
                        }

                        self.line_buffer_out.clear();
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
                addr_of_mut!(bytes_written),
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
        if self.process_id != 0 {
            Some(self.process_id)
        } else {
            None
        }
    }
}

impl Drop for Process {
    fn drop(&mut self) {
        self.close_handles();

        if self.h_process == INVALID_HANDLE_VALUE || !self.initialized {
            return;
        }

        let start = std::time::Instant::now();
        let kill_timeout = Duration::from_millis(100);

        loop {
            let mut exit_code: u32 = 0;
            unsafe { GetExitCodeProcess(self.h_process, addr_of_mut!(exit_code)) };

            if exit_code != STILL_ACTIVE {
                break;
            }

            if start.elapsed() >= kill_timeout {
                unsafe { TerminateProcess(self.h_process, 1) };
                break;
            }

            std::thread::sleep(Duration::from_millis(10));
        }

        unsafe {
            if self.h_process != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_process);
            }
            if self.h_thread != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_thread);
            }
        }
    }
}
