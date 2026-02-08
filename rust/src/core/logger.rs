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
    fn write_str(&mut self, msg: &str) -> std::io::Result<()> {
        match self {
            LogWriter::Plain(file) => write!(file, "{}", msg),
            LogWriter::Gzip(encoder) => write!(encoder, "{}", msg),
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
    engine_coms: bool,
}

impl Default for Logger {
    fn default() -> Self {
        Self::new()
    }
}

// On Windows, thread IDs are small (3 digits)
// On Linux, thread IDs are large (20 digits)
#[cfg(target_os = "windows")]
const THREAD_ID_WIDTH: usize = 3;

#[cfg(not(target_os = "windows"))]
const THREAD_ID_WIDTH: usize = 20;

impl Logger {
    pub fn new() -> Self {
        Self {
            should_log: AtomicBool::new(false),
            inner: Mutex::new(LoggerInner {
                level: LogLevel::Warn,
                writer: None,
                engine_coms: false,
            }),
        }
    }

    /// Configure the logger with a file, level, and compression settings.
    pub fn setup(
        &self,
        file_path: &str,
        level: LogLevel,
        append: bool,
        compress: bool,
        engine_coms: bool,
    ) {
        let mut inner = self.inner.lock().unwrap();
        inner.level = level;
        inner.engine_coms = engine_coms;

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
        self.write_to_file_formatted(level, msg, false);
    }

    /// Write a log message (engine communication, trace, etc.) to the log file only.
    pub fn log(&self, level: LogLevel, msg: &str) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }
        self.write_to_file_formatted(level, msg, false);
    }

    /// Log engine-to-host communication (engine output).
    pub fn read_from_engine(
        &self,
        line: &str,
        timestamp: &str,
        engine_name: &str,
        is_err: bool,
        thread_id: Option<u64>,
    ) {
        let inner = self.inner.lock().unwrap();
        if !self.should_log.load(Ordering::Relaxed) || !inner.engine_coms {
            return;
        }

        let thread_id_str = thread_id.map(|id| id.to_string()).unwrap_or_else(|| {
            format!("{:?}", std::thread::current().id())
                .trim_start_matches("ThreadId(")
                .trim_end_matches(')')
                .to_string()
        });

        let stderr_prefix = if is_err { "<stderr> " } else { "" };
        let prefix = self.make_prefix("Engine", timestamp, &thread_id_str);
        let formatted_msg = format!(
            "{} {} {} ---> {}\n",
            prefix, stderr_prefix, engine_name, line
        );

        drop(inner);
        self.write_raw(&formatted_msg);
    }

    /// Log host-to-engine communication (input sent to engine).
    pub fn write_to_engine(
        &self,
        input: &str,
        timestamp: &str,
        engine_name: &str,
        thread_id: Option<u64>,
    ) {
        let inner = self.inner.lock().unwrap();
        if !self.should_log.load(Ordering::Relaxed) || !inner.engine_coms {
            return;
        }

        // C++ behavior: if timestamp is empty, generate a new one
        let actual_timestamp = if timestamp.is_empty() {
            get_precise_timestamp()
        } else {
            timestamp.to_string()
        };

        let thread_id_str = thread_id.map(|id| id.to_string()).unwrap_or_else(|| {
            format!("{:?}", std::thread::current().id())
                .trim_start_matches("ThreadId(")
                .trim_end_matches(')')
                .to_string()
        });

        let prefix = self.make_prefix("Engine", &actual_timestamp, &thread_id_str);
        let formatted_msg = format!("{}  {} <--- {}\n", prefix, engine_name, input);

        drop(inner);
        self.write_raw(&formatted_msg);
    }

    /// Format: [label] [timestamp] <thread_id>
    fn make_prefix(&self, label: &str, timestamp: &str, thread_id: &str) -> String {
        format!(
            "[{:<6}] [{:>15}] <{:>width$}>",
            label,
            timestamp,
            thread_id,
            width = THREAD_ID_WIDTH
        )
    }

    /// Write a formatted log message with level, timestamp, and optional thread ID.
    fn write_to_file_formatted(&self, level: LogLevel, msg: &str, include_thread: bool) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }

        let inner = self.inner.lock().unwrap();
        if level < inner.level {
            return;
        }

        let label = match level {
            LogLevel::Trace => "TRACE",
            LogLevel::Debug => "DEBUG",
            LogLevel::Info => "INFO",
            LogLevel::Warn => "WARN",
            LogLevel::Error => "ERROR",
            LogLevel::Fatal => "FATAL",
        };

        let timestamp = get_precise_timestamp();
        let thread_id_str = if include_thread {
            format!("{:?}", std::thread::current().id())
                .trim_start_matches("ThreadId(")
                .trim_end_matches(')')
                .to_string()
        } else {
            String::new()
        };

        let prefix = self.make_prefix(label, &timestamp, &thread_id_str);
        let formatted_msg = format!("{} fastchess --- {}\n", prefix, msg);

        if let Some(ref _writer) = inner.writer.as_ref() {
            // Need to get mutable access, so we drop the inner guard and re-acquire
            drop(inner);
            self.write_raw(&formatted_msg);
        }
    }

    /// Write raw string to the log file (assumes formatting is already done).
    fn write_raw(&self, msg: &str) {
        let mut inner = self.inner.lock().unwrap();
        if let Some(ref mut writer) = inner.writer {
            let _ = writer.write_str(msg);
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

/// Get a precise timestamp in the format HH:MM:SS.microseconds
/// Uses local timezone to match C++ behavior.
fn get_precise_timestamp() -> String {
    use chrono::Local;

    let now = Local::now();
    let time_str = now.format("%H:%M:%S").to_string();
    let micros = now.timestamp_subsec_micros();
    format!("{}.{:06}", time_str, micros)
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
