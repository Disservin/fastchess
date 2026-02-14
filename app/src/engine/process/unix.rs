//! Unix-specific process implementation using poll() and file descriptors.

use std::io::Write;
use std::os::fd::{AsFd, AsRawFd, BorrowedFd, FromRawFd, OwnedFd};
use std::process::{Child, Command, Stdio};
use std::time::Duration;

use nix::poll::{poll, PollFd, PollFlags, PollTimeout};
use nix::unistd;

use super::common::{Line, ProcessResult, Standard};

/// Manages an engine child process with pipe-based I/O on Unix systems.
///
/// Uses `poll()` (not `select()`) for I/O multiplexing to avoid
/// the 1024 file descriptor limit that `select()` has.
pub struct Process {
    child: Option<Child>,
    interrupt_fds: Option<InterruptFds>,
    realtime_logging: bool,
    log_name: String,
    line_buffer_out: String,
    line_buffer_err: String,
    initialized: bool,
}

/// Owned file descriptors for the interrupt mechanism.
enum InterruptFds {
    #[cfg(target_os = "linux")]
    EventFd(OwnedFd),
    Pipe {
        read: OwnedFd,
        write: OwnedFd,
    },
}

impl InterruptFds {
    fn read_fd(&self) -> BorrowedFd<'_> {
        match self {
            #[cfg(target_os = "linux")]
            Self::EventFd(fd) => fd.as_fd(),
            Self::Pipe { read, .. } => read.as_fd(),
        }
    }

    fn write_fd(&self) -> BorrowedFd<'_> {
        match self {
            #[cfg(target_os = "linux")]
            Self::EventFd(fd) => fd.as_fd(),
            Self::Pipe { write, .. } => write.as_fd(),
        }
    }
}

impl Default for Process {
    fn default() -> Self {
        Self::new()
    }
}

/// Process raw bytes into complete lines, pushing them to `lines`.
/// Returns `Some(ProcessResult::ok())` if `searchword` is found at the start of a line.
fn drain_lines(
    buf: &[u8],
    line_buffer: &mut String,
    lines: &mut Vec<Line>,
    std: Standard,
    searchword: Option<&str>,
    realtime_logging: bool,
    log_name: &str,
) -> Option<ProcessResult> {
    let is_stderr = matches!(std, Standard::Err);

    for &byte in buf {
        if byte != b'\n' {
            line_buffer.push(byte as char);
            continue;
        }

        if line_buffer.is_empty() {
            continue;
        }

        let line = line_buffer.trim_end_matches('\r').to_string();
        let ts = crate::core::datetime_precise();

        if realtime_logging {
            crate::core::logger::LOGGER.read_from_engine(&line, &ts, log_name, is_stderr, None);
        }

        let found = searchword
            .filter(|sw| !sw.is_empty())
            .is_some_and(|sw| line.starts_with(sw));

        lines.push(Line {
            line,
            time: ts,
            std,
        });
        line_buffer.clear();

        if found {
            return Some(ProcessResult::ok());
        }
    }

    None
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
            cmd.args(args.split_whitespace());
        }

        cmd.stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        let child = match cmd.spawn() {
            Ok(c) => c,
            Err(e) => return ProcessResult::error(format!("Failed to spawn '{command}': {e}")),
        };

        self.setup_interrupt();
        self.child = Some(child);
        self.initialized = true;
        ProcessResult::ok()
    }

    pub fn alive(&mut self) -> ProcessResult {
        let child = match self.child.as_mut() {
            Some(c) => c,
            None => return ProcessResult::error("Process not initialized"),
        };

        match child.try_wait() {
            Ok(None) => ProcessResult::ok(),
            Ok(Some(status)) => {
                ProcessResult::error(format!("Process exited with status: {status}"))
            }
            Err(e) => ProcessResult::error(format!("Failed to check process: {e}")),
        }
    }

    pub fn setup_read(&mut self) {
        self.line_buffer_out.clear();
        self.line_buffer_err.clear();
    }

    pub fn read_output(
        &mut self,
        lines: &mut Vec<Line>,
        searchword: &str,
        timeout: Duration,
    ) -> ProcessResult {
        lines.clear();

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

        // Capture raw fds up front to avoid borrow conflicts during the loop.
        let interrupt_raw_fd = self.interrupt_fds.as_ref().map(|f| f.read_fd().as_raw_fd());

        let mut read_buf = [0u8; 4096];

        loop {
            // Build poll_fds from raw fds — no borrows on self/stdout/stderr held.
            // let mut poll_fds = vec![unsafe {
            //     PollFd::new(
            //         BorrowedFd::borrow_raw(stdout_raw_fd),
            //         PollFlags::POLLIN | PollFlags::POLLERR | PollFlags::POLLHUP,
            //     )
            // }];
            let mut poll_fds = vec![PollFd::new(
                stdout.as_fd(),
                PollFlags::POLLIN | PollFlags::POLLERR | PollFlags::POLLHUP,
            )];

            if let Some(ifd) = interrupt_raw_fd {
                poll_fds
                    .push(unsafe { PollFd::new(BorrowedFd::borrow_raw(ifd), PollFlags::POLLIN) });
            }

            let stderr_idx = poll_fds.len();
            poll_fds.push(PollFd::new(
                stderr.as_fd(),
                PollFlags::POLLIN | PollFlags::POLLERR | PollFlags::POLLHUP,
            ));

            let ready = match poll(&mut poll_fds, poll_timeout) {
                Ok(n) => n,
                Err(e) => return ProcessResult::error(format!("poll failed: {e}")),
            };

            // Timeout
            if ready == 0 {
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
            if let Some(ifd) = interrupt_raw_fd {
                if let Some(revents) = poll_fds[1].revents() {
                    if revents.contains(PollFlags::POLLIN) {
                        let mut junk = [0u8; 8];
                        let _ = unistd::read(ifd, &mut junk);
                        return ProcessResult::error("Interrupted by control pipe");
                    }
                }
            }

            // Check stdout
            if let Some(revents) = poll_fds[0].revents() {
                if revents.intersects(PollFlags::POLLHUP | PollFlags::POLLERR) {
                    return ProcessResult::error("Engine crashed (stdout)");
                }

                if revents.contains(PollFlags::POLLIN) {
                    let n = match unistd::read(stdout.as_raw_fd(), &mut read_buf) {
                        Ok(0) => return ProcessResult::error("EOF on stdout"),
                        Ok(n) => n,
                        Err(_) => return ProcessResult::error("read failed on stdout"),
                    };

                    if let Some(r) = drain_lines(
                        &read_buf[..n],
                        &mut self.line_buffer_out,
                        lines,
                        Standard::Output,
                        Some(searchword),
                        self.realtime_logging,
                        &self.log_name,
                    ) {
                        return r;
                    }
                }
            }

            // Check stderr
            if let Some(revents) = poll_fds[stderr_idx].revents() {
                if revents.contains(PollFlags::POLLIN) {
                    if let Ok(n) = unistd::read(stderr.as_raw_fd(), &mut read_buf) {
                        drain_lines(
                            &read_buf[..n],
                            &mut self.line_buffer_err,
                            lines,
                            Standard::Err,
                            None,
                            self.realtime_logging,
                            &self.log_name,
                        );
                    }
                }

                if revents.intersects(PollFlags::POLLHUP | PollFlags::POLLERR) {
                    return ProcessResult::error("Engine crashed (stderr)");
                }
            }
        }
    }

    pub fn write_input(&mut self, input: &str) -> ProcessResult {
        let child = match self.child.as_mut() {
            Some(c) => c,
            None => return ProcessResult::error("Process not initialized"),
        };

        let stdin = match child.stdin.as_mut() {
            Some(s) => s,
            None => return ProcessResult::error("No stdin pipe"),
        };

        if let Err(e) = stdin.write_all(input.as_bytes()) {
            return ProcessResult::error(format!("write failed: {e}"));
        }

        let _ = stdin.flush();
        ProcessResult::ok()
    }

    #[cfg(target_os = "linux")]
    pub fn set_affinity(&self, cpus: &[i32]) -> bool {
        use nix::sched::{sched_setaffinity, CpuSet};
        use nix::unistd::Pid;

        let Some(ref child) = self.child else {
            return false;
        };
        let pid = Pid::from_raw(child.id() as i32);

        let mut cpu_set = CpuSet::new();
        for &cpu in cpus {
            if cpu_set.set(cpu as usize).is_err() {
                return false;
            }
        }

        sched_setaffinity(pid, &cpu_set).is_ok()
    }

    #[cfg(not(target_os = "linux"))]
    pub fn set_affinity(&self, _cpus: &[i32]) -> bool {
        false
    }

    fn setup_interrupt(&mut self) {
        #[cfg(target_os = "linux")]
        {
            use nix::sys::eventfd::{EfdFlags, EventFd};
            if let Ok(efd) = EventFd::from_value_and_flags(0, EfdFlags::EFD_CLOEXEC) {
                let raw_fd = efd.as_raw_fd();
                let owned_fd = unsafe { OwnedFd::from_raw_fd(raw_fd) };
                std::mem::forget(efd);
                self.interrupt_fds = Some(InterruptFds::EventFd(owned_fd));
                return;
            }
        }

        if let Ok((read_fd, write_fd)) = unistd::pipe() {
            self.interrupt_fds = Some(InterruptFds::Pipe {
                read: read_fd,
                write: write_fd,
            });
        }
    }

    pub fn interrupt(&self) {
        let Some(ref fds) = self.interrupt_fds else {
            return;
        };
        let val: u64 = 1;
        let _ = unistd::write(fds.write_fd(), &val.to_ne_bytes());
    }

    pub fn pid(&self) -> Option<u32> {
        self.child.as_ref().map(|c| c.id())
    }
}

impl Drop for Process {
    fn drop(&mut self) {
        let Some(ref mut child) = self.child else {
            return;
        };

        if let Some(ref mut stdin) = child.stdin {
            let _ = stdin.write_all(b"stop\n");
            let _ = stdin.write_all(b"quit\n");
            let _ = stdin.flush();
        }

        let start = std::time::Instant::now();
        loop {
            match child.try_wait() {
                Ok(Some(_)) | Err(_) => break,
                Ok(None) if start.elapsed() > Duration::from_secs(5) => {
                    let _ = child.kill();
                    let _ = child.wait();
                    break;
                }
                Ok(None) => std::thread::sleep(Duration::from_millis(50)),
            }
        }
    }
}
