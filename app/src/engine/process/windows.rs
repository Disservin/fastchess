//! Windows-specific process implementation using named pipes and overlapped I/O.

use std::mem::zeroed;
use std::ptr::{addr_of_mut, null, null_mut};
use std::time::Duration;

use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, HANDLE, INVALID_HANDLE_VALUE, WAIT_OBJECT_0, WAIT_TIMEOUT,
};
use windows_sys::Win32::Foundation::{SetHandleInformation, HANDLE_FLAG_INHERIT};
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

use super::common::{Line, ProcessResult, Standard};

/// Buffer size for reading from pipes.
const BUFFER_SIZE: usize = 4096;

/// Pipe access mode constants
const PIPE_ACCESS_INBOUND: u32 = 0x00000001;
const PIPE_TYPE_BYTE: u32 = 0x00000000;
const PIPE_WAIT: u32 = 0x00000000;

/// File access constants
const GENERIC_WRITE: u32 = 0x40000000;

/// Process exit code indicating still active
const STILL_ACTIVE: u32 = 259;

/// Manages an engine child process with pipe-based I/O on Windows.
///
/// Uses named pipes with overlapped I/O for asynchronous reading,
/// similar to the original C++ implementation.
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

// Windows HANDLEs are thread-safe and can be used across threads
unsafe impl Send for Process {}

impl Default for Process {
    fn default() -> Self {
        Self::new()
    }
}

impl Process {
    pub fn new() -> Self {
        Self {
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
        }
    }

    pub fn set_realtime_logging(&mut self, rt: bool) {
        self.realtime_logging = rt;
    }

    /// Initialize the process: spawn the engine binary and set up pipes.
    pub fn init(
        &mut self,
        working_dir: &str,
        command: &str,
        args: &str,
        log_name: &str,
    ) -> ProcessResult {
        self.log_name = log_name.to_string();

        // Create pipes for stdout and stdin using named pipes (allows overlapped I/O)
        if !self.create_pipes() {
            self.close_handles();
            return ProcessResult::error("Failed to create pipes");
        }

        // Prevent parent handles from being inherited by child
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

        // Build command line
        let cmd_line = if args.is_empty() {
            command.to_string()
        } else {
            format!("{} {}", command, args)
        };
        let cmd_cstring = std::ffi::CString::new(cmd_line).unwrap();

        // Set up STARTUPINFOA
        let mut si: STARTUPINFOA = unsafe { zeroed() };
        si.cb = std::mem::size_of::<STARTUPINFOA>() as u32;
        si.hStdOutput = self.h_stdout_write;
        si.hStdInput = self.h_stdin_read;
        si.hStdError = self.h_stdout_write; // Redirect stderr to stdout
        si.dwFlags = STARTF_USESTDHANDLES;

        // Working directory
        let wd_cstring = if working_dir.is_empty() || working_dir == "." {
            None
        } else {
            Some(std::ffi::CString::new(working_dir).unwrap())
        };
        let wd_ptr = wd_cstring
            .as_ref()
            .map_or(null(), |s| s.as_ptr() as *const u8);

        // Create process
        let mut pi: PROCESS_INFORMATION = unsafe { zeroed() };
        let success = unsafe {
            CreateProcessA(
                null(),                          // Application name
                cmd_cstring.as_ptr() as *mut u8, // Command line
                null_mut(),                      // Process security attributes
                null_mut(),                      // Thread security attributes
                1,                               // Inherit handles
                0x00000200,                      // CREATE_NEW_PROCESS_GROUP
                null_mut(),                      // Environment
                wd_ptr,                          // Working directory
                addr_of_mut!(si),                // Startup info
                addr_of_mut!(pi),                // Process info
            )
        };

        if success == 0 {
            self.close_handles();
            return ProcessResult::error(format!("Failed to create process: {}", unsafe {
                GetLastError()
            }));
        }

        // Store process handles
        self.h_process = pi.hProcess;
        self.h_thread = pi.hThread;
        self.process_id = pi.dwProcessId;

        // Close child-side handles in parent process
        unsafe {
            CloseHandle(self.h_stdout_write);
            CloseHandle(self.h_stdin_read);
        }
        self.h_stdout_write = INVALID_HANDLE_VALUE;
        self.h_stdin_read = INVALID_HANDLE_VALUE;

        self.initialized = true;
        ProcessResult::ok()
    }

    /// Create named pipes for stdin/stdout with overlapped I/O support.
    fn create_pipes(&mut self) -> bool {
        use std::sync::atomic::{AtomicU32, Ordering};
        static PIPE_SERIAL: AtomicU32 = AtomicU32::new(0);

        let pid = unsafe { windows_sys::Win32::System::Threading::GetCurrentProcessId() };
        let serial = PIPE_SERIAL.fetch_add(1, Ordering::SeqCst);

        // Create stdout pipe (child writes, parent reads)
        let pipe_name_out = format!("\\\\.\\Pipe\\Fastchess.{:08x}.{:08x}.out", pid, serial);
        let pipe_name_out_c = std::ffi::CString::new(&*pipe_name_out).unwrap();

        self.h_stdout_read = unsafe {
            CreateNamedPipeA(
                pipe_name_out_c.as_ptr() as *const u8,
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_WAIT,
                1,                  // Max instances
                BUFFER_SIZE as u32, // Output buffer size
                BUFFER_SIZE as u32, // Input buffer size
                120_000,            // Timeout in ms
                null_mut(),         // Security attributes
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
                null_mut(),
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

        // Create stdin pipe (parent writes, child reads)
        let pipe_name_in = format!("\\\\.\\Pipe\\Fastchess.{:08x}.{:08x}.in", pid, serial);
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
                null_mut(),
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
                null_mut(),
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

    /// Check if the process is still alive.
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

    /// Clear line buffers before a new read cycle.
    pub fn setup_read(&mut self) {
        self.line_buffer_out.clear();
    }

    /// Read output from the engine using overlapped I/O.
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

        let mut buffer = [0u8; BUFFER_SIZE];

        loop {
            let elapsed = start_time.elapsed();
            if elapsed >= timeout && !timeout.is_zero() {
                return ProcessResult::timeout();
            }

            // Create event for overlapped I/O
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
                if error == 997 {
                    // ERROR_IO_PENDING - wait for completion
                    let remaining_ms = if timeout.is_zero() {
                        u32::MAX
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
                        unsafe {
                            CloseHandle(event);
                        }
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
                        unsafe {
                            CloseHandle(event);
                        }
                        return ProcessResult::error("Failed to get overlapped result");
                    }
                } else {
                    unsafe {
                        CloseHandle(event);
                    }
                    return ProcessResult::error(format!("Read failed with error: {}", error));
                }
            }

            unsafe {
                CloseHandle(event);
            }

            if bytes_read == 0 {
                continue;
            }

            // Process the bytes into lines
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

                        let found =
                            !searchword.is_empty() && self.line_buffer_out.starts_with(searchword);

                        lines.push(Line {
                            line: self.line_buffer_out.clone(),
                            time: ts,
                            std: Standard::Output,
                        });

                        self.line_buffer_out.clear();

                        if found {
                            return ProcessResult::ok();
                        }
                    }
                } else {
                    self.line_buffer_out.push(c);
                }
            }
        }
    }

    /// Write input to the engine's stdin.
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

    /// Set CPU affinity for the engine process (not implemented on Windows).
    pub fn set_affinity(&self, _cpus: &[i32]) -> bool {
        // Could be implemented using SetProcessAffinityMask on Windows
        false
    }

    /// Interrupt any blocking operations (no-op on Windows for now).
    pub fn interrupt(&self) {
        // Not implemented - would require signaling mechanism
    }

    /// Get the process ID, if running.
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
        // Send quit commands
        if self.h_stdin_write != INVALID_HANDLE_VALUE {
            let _ = self.write_input("stop\nquit\n");
        }

        // Wait for process to terminate gracefully
        let start = std::time::Instant::now();
        loop {
            let mut exit_code: u32 = 0;
            let result = unsafe { GetExitCodeProcess(self.h_process, addr_of_mut!(exit_code)) };

            if result == 0 || exit_code != STILL_ACTIVE {
                break; // Process terminated
            }

            if start.elapsed() > Duration::from_secs(5) {
                // Force terminate
                unsafe {
                    TerminateProcess(self.h_process, 1);
                }
                break;
            }

            std::thread::sleep(Duration::from_millis(50));
        }

        // Clean up handles
        unsafe {
            if self.h_process != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_process);
            }
            if self.h_thread != INVALID_HANDLE_VALUE {
                CloseHandle(self.h_thread);
            }
        }

        self.close_handles();
    }
}
