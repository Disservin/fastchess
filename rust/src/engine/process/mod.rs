use std::io::Write;
use std::os::fd::{AsFd, AsRawFd, BorrowedFd, OwnedFd};
use std::process::{Child, Command, Stdio};
use std::time::Duration;

use nix::poll::{poll, PollFd, PollFlags, PollTimeout};
use nix::unistd;

/// Which standard stream a line came from.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Standard {
    Input,
    Output,
    Err,
}

/// Status of a process operation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Status {
    Ok,
    Err,
    Timeout,
    None,
}

/// Result of a process operation.
#[derive(Debug, Clone)]
pub struct ProcessResult {
    pub code: Status,
    pub message: String,
}

impl ProcessResult {
    pub fn ok() -> Self {
        Self {
            code: Status::Ok,
            message: String::new(),
        }
    }

    pub fn error(msg: impl Into<String>) -> Self {
        Self {
            code: Status::Err,
            message: msg.into(),
        }
    }

    pub fn timeout() -> Self {
        Self {
            code: Status::Timeout,
            message: "timeout".to_string(),
        }
    }

    pub fn is_ok(&self) -> bool {
        self.code == Status::Ok
    }
}

/// A line of output from an engine process.
#[derive(Debug, Clone)]
pub struct Line {
    pub line: String,
    pub time: String,
    pub std: Standard,
}

/// Manages an engine child process with pipe-based I/O.
///
/// Uses `poll()` (not `select()`) for I/O multiplexing to avoid
/// the 1024 file descriptor limit that `select()` has. This is important
/// because even moderate concurrency levels can exhaust that limit.
///
/// **Design note on threading**: Unlike the C++ version which spawns via
/// `posix_spawn`, we use `std::process::Command` which handles pipe setup
/// and process spawning. The I/O reading uses synchronous `poll()` on the
/// calling thread rather than spawning additional reader threads, keeping
/// the file descriptor count low.
pub struct Process {
    child: Option<Child>,
    // We no longer store raw i32 FDs to avoid safety issues.
    // The pipes are owned by the `child` field.
    interrupt_fds: Option<InterruptFds>,
    realtime_logging: bool,
    log_name: String,
    line_buffer_out: String,
    line_buffer_err: String,
    initialized: bool,
}

/// Owned file descriptors for the interrupt mechanism.
/// Automatically closed on drop.
enum InterruptFds {
    /// Linux eventfd - single fd for both read and write
    EventFd(OwnedFd),
    /// Fallback pipe - separate read and write fds
    Pipe { read: OwnedFd, write: OwnedFd },
}

impl InterruptFds {
    /// Get a borrowed fd for reading (used in poll).
    fn read_fd(&self) -> BorrowedFd<'_> {
        match self {
            InterruptFds::EventFd(fd) => fd.as_fd(),
            InterruptFds::Pipe { read, .. } => read.as_fd(),
        }
    }

    /// Get a borrowed fd for writing (used in interrupt).
    fn write_fd(&self) -> BorrowedFd<'_> {
        match self {
            InterruptFds::EventFd(fd) => fd.as_fd(),
            InterruptFds::Pipe { write, .. } => write.as_fd(),
        }
    }
}

impl Default for Process {
    fn default() -> Self {
        Self::new()
    }
}

impl Process {
    pub fn new() -> Self {
        Self {
            child: None,
            interrupt_fds: None,
            realtime_logging: true,
            log_name: String::new(),
            line_buffer_out: String::with_capacity(300),
            line_buffer_err: String::with_capacity(300),
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

        let mut cmd = Command::new(command);

        if !working_dir.is_empty() && working_dir != "." {
            cmd.current_dir(working_dir);
        }

        if !args.is_empty() {
            // Split args respecting quotes (simplified)
            for arg in args.split_whitespace() {
                cmd.arg(arg);
            }
        }

        cmd.stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        match cmd.spawn() {
            Ok(child) => {
                self.setup_interrupt();
                self.child = Some(child);
                self.initialized = true;

                ProcessResult::ok()
            }
            Err(e) => ProcessResult::error(format!("Failed to spawn '{}': {}", command, e)),
        }
    }

    /// Check if the process is still alive.
    pub fn alive(&mut self) -> ProcessResult {
        if let Some(ref mut child) = self.child {
            match child.try_wait() {
                Ok(None) => ProcessResult::ok(), // still running
                Ok(Some(status)) => {
                    ProcessResult::error(format!("Process exited with status: {}", status))
                }
                Err(e) => ProcessResult::error(format!("Failed to check process: {}", e)),
            }
        } else {
            ProcessResult::error("Process not initialized")
        }
    }

    /// Clear line buffers before a new read cycle.
    pub fn setup_read(&mut self) {
        self.line_buffer_out.clear();
        self.line_buffer_err.clear();
    }

    /// Read output from the engine until `searchword` is found at the start of a line,
    /// or until the timeout is reached.
    ///
    /// Uses `poll()` for I/O multiplexing, which has no file descriptor limit
    /// (unlike `select()` which is limited to FD 1024).
    pub fn read_output(
        &mut self,
        lines: &mut Vec<Line>,
        searchword: &str,
        timeout: Duration,
    ) -> ProcessResult {
        lines.clear();

        // Ensure the child and its stdout are available
        let child = match self.child.as_mut() {
            Some(c) => c,
            None => return ProcessResult::error("Process not initialized"),
        };

        let stdout = child.stdout.as_mut().expect("Stdout not piped");
        let stderr = child.stderr.as_mut().expect("Stderr not piped");

        let poll_timeout = if timeout.is_zero() {
            PollTimeout::NONE
        } else {
            PollTimeout::try_from(timeout).unwrap_or(PollTimeout::MAX)
        };

        let mut read_buf = [0u8; 4096];

        loop {
            // Build pollfd array
            let mut poll_fds = vec![PollFd::new(
                stdout.as_fd(),
                PollFlags::POLLIN | PollFlags::POLLERR | PollFlags::POLLHUP,
            )];

            // Interrupt fd
            if let Some(ref interrupt) = self.interrupt_fds {
                poll_fds.push(PollFd::new(interrupt.read_fd(), PollFlags::POLLIN));
            }

            // Stderr fd
            poll_fds.push(PollFd::new(
                stderr.as_fd(),
                PollFlags::POLLIN | PollFlags::POLLERR | PollFlags::POLLHUP,
            ));

            let stdout_idx = 0;
            let interrupt_idx = if self.interrupt_fds.is_some() {
                Some(1)
            } else {
                None
            };
            let stderr_idx = poll_fds.len() - 1;

            let ready = match poll(&mut poll_fds, poll_timeout) {
                Ok(n) => n,
                Err(e) => return ProcessResult::error(format!("poll failed: {}", e)),
            };

            if ready == 0 {
                // Timeout - flush any partial lines
                if !self.line_buffer_out.is_empty() {
                    let ts = crate::core::datetime_precise();
                    lines.push(Line {
                        line: std::mem::take(&mut self.line_buffer_out),
                        time: ts,
                        std: Standard::Output,
                    });
                }
                return ProcessResult::timeout();
            }

            // Check interrupt
            if self.interrupt_fds.is_some() && poll_fds.len() > 1 {
                if let Some(revents) = poll_fds[interrupt_idx.unwrap()].revents() {
                    if revents.contains(PollFlags::POLLIN) {
                        // Drain the interrupt fd
                        if let Some(ref interrupt) = self.interrupt_fds {
                            let mut junk = [0u8; 8];
                            let _ = unistd::read(interrupt.read_fd().as_raw_fd(), &mut junk);
                        }
                        return ProcessResult::error("Interrupted by control pipe");
                    }
                }
            }

            // Check stdout
            if let Some(revents) = poll_fds[stdout_idx].revents() {
                if revents.contains(PollFlags::POLLHUP) || revents.contains(PollFlags::POLLERR) {
                    return ProcessResult::error("Engine crashed (stdout)");
                }
                if revents.contains(PollFlags::POLLIN) {
                    let bytes_read = match unistd::read(stdout.as_fd().as_raw_fd(), &mut read_buf) {
                        Ok(n) => n,
                        Err(_) => return ProcessResult::error("read failed on stdout"),
                    };

                    if bytes_read == 0 {
                        return ProcessResult::error("EOF on stdout");
                    }

                    // Process bytes into lines
                    for &byte in &read_buf[..bytes_read] {
                        let c = byte as char;
                        if c == '\n' {
                            if !self.line_buffer_out.is_empty() {
                                // Strip trailing \r
                                let line = self.line_buffer_out.trim_end_matches('\r').to_string();
                                let ts = crate::core::datetime_precise();

                                if self.realtime_logging {
                                    crate::core::logger::LOGGER.read_from_engine(
                                        &line,
                                        &ts,
                                        &self.log_name,
                                        false,
                                        None,
                                    );
                                }

                                let found = !searchword.is_empty() && line.starts_with(searchword);

                                lines.push(Line {
                                    line,
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

            // Check stderr
            if let Some(revents) = poll_fds[stderr_idx].revents() {
                if revents.contains(PollFlags::POLLIN) {
                    let err_fd = stderr.as_fd().as_raw_fd();
                    let bytes_read = match unistd::read(err_fd, &mut read_buf) {
                        Ok(n) => n,
                        Err(_) => continue,
                    };

                    for &byte in &read_buf[..bytes_read] {
                        let c = byte as char;
                        if c == '\n' {
                            if !self.line_buffer_err.is_empty() {
                                let line = self.line_buffer_err.trim_end_matches('\r').to_string();
                                let ts = crate::core::datetime_precise();

                                if self.realtime_logging {
                                    crate::core::logger::LOGGER.read_from_engine(
                                        &line,
                                        &ts,
                                        &self.log_name,
                                        true,
                                        None,
                                    );
                                }

                                lines.push(Line {
                                    line,
                                    time: ts,
                                    std: Standard::Err,
                                });

                                self.line_buffer_err.clear();
                            }
                        } else {
                            self.line_buffer_err.push(c);
                        }
                    }
                }
                if revents.contains(PollFlags::POLLHUP) || revents.contains(PollFlags::POLLERR) {
                    return ProcessResult::error("Engine crashed (stderr)");
                }
            }
        }
    }

    /// Write input to the engine's stdin.
    pub fn write_input(&mut self, input: &str) -> ProcessResult {
        if let Some(ref mut child) = self.child {
            if let Some(ref mut stdin) = child.stdin {
                match stdin.write_all(input.as_bytes()) {
                    Ok(_) => {
                        let _ = stdin.flush();
                        ProcessResult::ok()
                    }
                    Err(e) => ProcessResult::error(format!("write failed: {}", e)),
                }
            } else {
                ProcessResult::error("No stdin pipe")
            }
        } else {
            ProcessResult::error("Process not initialized")
        }
    }

    /// Set CPU affinity for the engine process (Linux only).
    #[cfg(target_os = "linux")]
    pub fn set_affinity(&self, cpus: &[i32]) -> bool {
        use nix::sched::{sched_setaffinity, CpuSet};
        use nix::unistd::Pid;

        if let Some(ref child) = self.child {
            let pid = Pid::from_raw(child.id() as i32);
            let mut cpu_set = CpuSet::new();
            for &cpu in cpus {
                if cpu_set.set(cpu as usize).is_err() {
                    return false;
                }
            }
            sched_setaffinity(pid, &cpu_set).is_ok()
        } else {
            false
        }
    }

    #[cfg(not(target_os = "linux"))]
    pub fn set_affinity(&self, _cpus: &[i32]) -> bool {
        false
    }

    fn setup_interrupt(&mut self) {
        // Try eventfd first (Linux), fall back to pipe
        #[cfg(target_os = "linux")]
        {
            use nix::sys::eventfd::{EfdFlags, EventFd};
            if let Ok(efd) = EventFd::from_value_and_flags(0, EfdFlags::EFD_CLOEXEC) {
                // Convert EventFd to OwnedFd
                let owned_fd: OwnedFd = efd.into();
                self.interrupt_fds = Some(InterruptFds::EventFd(owned_fd));
                return;
            }
        }

        // Fallback: pipe
        if let Ok((read_fd, write_fd)) = unistd::pipe() {
            self.interrupt_fds = Some(InterruptFds::Pipe {
                read: read_fd,
                write: write_fd,
            });
        }
    }

    /// Signal the interrupt pipe to wake up any blocking poll().
    pub fn interrupt(&self) {
        if let Some(ref fds) = self.interrupt_fds {
            let val: u64 = 1;
            let _ = unistd::write(fds.write_fd(), &val.to_ne_bytes());
        }
    }

    /// Get the process ID, if running.
    pub fn pid(&self) -> Option<u32> {
        self.child.as_ref().map(|c| c.id())
    }
}

impl Drop for Process {
    fn drop(&mut self) {
        // Send quit signal and clean up
        // Write directly to stdin to avoid borrow issues with self.write_input()
        if let Some(ref mut child) = self.child {
            if let Some(ref mut stdin) = child.stdin {
                let _ = stdin.write_all(b"stop\n");
                let _ = stdin.write_all(b"quit\n");
                let _ = stdin.flush();
            }

            // Wait with timeout, then kill
            let start = std::time::Instant::now();
            loop {
                match child.try_wait() {
                    Ok(Some(_)) => break,
                    Ok(None) => {
                        if start.elapsed() > Duration::from_secs(5) {
                            let _ = child.kill();
                            let _ = child.wait();
                            break;
                        }
                        std::thread::sleep(Duration::from_millis(50));
                    }
                    Err(_) => break,
                }
            }
        }

        // interrupt_fds are automatically closed when dropped (OwnedFd)
    }
}
