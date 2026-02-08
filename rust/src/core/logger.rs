use std::io::Write;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

use crate::types::LogLevel;

/// Global logger instance.
///
/// The C++ codebase uses a static Logger with mutex-protected file writing.
/// In Rust we use a similar pattern with a lazily-initialized global.
pub static LOGGER: std::sync::LazyLock<Logger> = std::sync::LazyLock::new(Logger::new);

/// A writer that can be either a plain file or a gzip-compressed file.
enum LogWriter {
    Plain(std::fs::File),
    Gzip(flate2::write::GzEncoder<std::fs::File>),
}

impl LogWriter {
    fn write_line(&mut self, msg: &str) -> std::io::Result<()> {
        match self {
            LogWriter::Plain(file) => writeln!(file, "{}", msg),
            LogWriter::Gzip(encoder) => writeln!(encoder, "{}", msg),
        }
    }

    /// Flush the writer (important for gzip to ensure data is written).
    fn flush(&mut self) -> std::io::Result<()> {
        match self {
            LogWriter::Plain(file) => file.flush(),
            LogWriter::Gzip(encoder) => encoder.flush(),
        }
    }

    /// Finish writing (for gzip, this writes the trailer and finalizes the stream).
    fn finish(self) -> std::io::Result<()> {
        match self {
            LogWriter::Plain(mut file) => file.flush(),
            LogWriter::Gzip(encoder) => {
                encoder.finish()?;
                Ok(())
            }
        }
    }
}

/// Thread-safe logger with optional file output.
pub struct Logger {
    /// Whether logging is enabled at all (any log file configured).
    pub should_log: AtomicBool,
    inner: Mutex<LoggerInner>,
}

struct LoggerInner {
    level: LogLevel,
    writer: Option<LogWriter>,
}

impl Default for Logger {
    fn default() -> Self {
        Self::new()
    }
}

impl Logger {
    pub fn new() -> Self {
        Self {
            should_log: AtomicBool::new(false),
            inner: Mutex::new(LoggerInner {
                level: LogLevel::Warn,
                writer: None,
            }),
        }
    }

    /// Configure the logger with a file, level, and compression settings.
    pub fn setup(&self, file_path: &str, level: LogLevel, append: bool, compress: bool) {
        let mut inner = self.inner.lock().unwrap();
        inner.level = level;

        if !file_path.is_empty() {
            // Open the file
            let file = std::fs::OpenOptions::new()
                .create(true)
                .write(true)
                .append(append)
                .truncate(!append)
                .open(file_path);

            match file {
                Ok(f) => {
                    // Wrap in gzip encoder if compression is requested
                    let writer = if compress {
                        LogWriter::Gzip(flate2::write::GzEncoder::new(
                            f,
                            flate2::Compression::default(),
                        ))
                    } else {
                        LogWriter::Plain(f)
                    };

                    inner.writer = Some(writer);
                    self.should_log.store(true, Ordering::Relaxed);
                }
                Err(e) => {
                    eprintln!("Failed to open log file '{}': {}", file_path, e);
                }
            }
        }
    }

    /// Print a message to stdout (and optionally to the log file).
    pub fn print(&self, level: LogLevel, msg: &str) {
        // Always print to stdout for WARN and above
        if level >= LogLevel::Warn {
            eprintln!("{}", msg);
        } else {
            println!("{}", msg);
        }
        self.write_to_file(level, msg);
    }

    /// Write a log message (engine communication, trace, etc.) to the log file only.
    pub fn log(&self, level: LogLevel, msg: &str) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }
        self.write_to_file(level, msg);
    }

    /// Log engine-to-host communication.
    pub fn read_from_engine(&self, line: &str, timestamp: &str, engine_name: &str, is_err: bool) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }
        let prefix = if is_err { "stderr" } else { "stdout" };
        let msg = format!("[{}] <{} {}> {}", timestamp, engine_name, prefix, line);
        self.write_to_file(LogLevel::Trace, &msg);
    }

    /// Log host-to-engine communication.
    pub fn write_to_engine(&self, input: &str, timestamp: &str, engine_name: &str) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }
        let msg = format!("[{}] >{} {}", timestamp, engine_name, input);
        self.write_to_file(LogLevel::Trace, &msg);
    }

    fn write_to_file(&self, level: LogLevel, msg: &str) {
        let mut inner = self.inner.lock().unwrap();
        if level < inner.level {
            return;
        }
        if let Some(ref mut writer) = inner.writer {
            let _ = writer.write_line(msg);
            // Flush after each write to ensure data is written
            // (important for gzip to avoid truncation)
            let _ = writer.flush();
        }
    }

    /// Finalize the log writer (important for gzip compression).
    /// This should be called before the program exits to ensure the gzip trailer is written.
    pub fn finish(&self) {
        let mut inner = self.inner.lock().unwrap();
        if let Some(writer) = inner.writer.take() {
            let _ = writer.finish();
        }
    }
}

impl Drop for Logger {
    fn drop(&mut self) {
        // Try to finish the writer on drop
        if let Ok(mut inner) = self.inner.lock() {
            if let Some(writer) = inner.writer.take() {
                let _ = writer.finish();
            }
        }
    }
}

/// Convenience macros for logging at various levels.
#[macro_export]
macro_rules! log_trace {
    ($($arg:tt)*) => {
        $crate::core::logger::LOGGER.log($crate::types::LogLevel::Trace, &format!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_info {
    ($($arg:tt)*) => {
        $crate::core::logger::LOGGER.log($crate::types::LogLevel::Info, &format!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_warn {
    ($($arg:tt)*) => {
        $crate::core::logger::LOGGER.print($crate::types::LogLevel::Warn, &format!($($arg)*))
    };
}

#[macro_export]
macro_rules! log_fatal {
    ($($arg:tt)*) => {
        $crate::core::logger::LOGGER.print($crate::types::LogLevel::Fatal, &format!($($arg)*))
    };
}
