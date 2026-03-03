use std::io::Write;
use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::mpsc::{self, SyncSender};
use std::sync::Mutex;

use crate::types::LogLevel;

pub static LOGGER: std::sync::LazyLock<Logger> = std::sync::LazyLock::new(Logger::new);

use chrono::Local;
use std::io::BufWriter;

enum LogWriter {
    Plain(BufWriter<std::fs::File>),
    Gzip(flate2::write::GzEncoder<BufWriter<std::fs::File>>),
}

impl LogWriter {
    pub fn new(f: std::fs::File, compress: bool) -> std::io::Result<Self> {
        let buf = BufWriter::with_capacity(65536, f);
        if compress {
            Ok(LogWriter::Gzip(flate2::write::GzEncoder::new(
                buf,
                flate2::Compression::default(),
            )))
        } else {
            Ok(LogWriter::Plain(buf))
        }
    }

    fn write_str(&mut self, msg: &str) -> std::io::Result<()> {
        match self {
            LogWriter::Plain(file) => file.write_all(msg.as_bytes()),
            LogWriter::Gzip(encoder) => encoder.write_all(msg.as_bytes()),
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        match self {
            LogWriter::Plain(file) => file.flush(),
            LogWriter::Gzip(encoder) => encoder.flush(),
        }
    }

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

enum LogMessage {
    Write(String),
    #[allow(dead_code)]
    Flush,
    Finish,
}

struct LoggingThread {
    sender: SyncSender<LogMessage>,
    handle: Option<std::thread::JoinHandle<()>>,
}

impl LoggingThread {
    fn new(mut writer: LogWriter) -> Self {
        let (sender, receiver) = mpsc::sync_channel::<LogMessage>(4096);

        let handle = std::thread::Builder::new()
            .name("logger".to_string())
            .spawn(move || {
                for msg in receiver {
                    match msg {
                        LogMessage::Write(s) => {
                            let _ = writer.write_str(&s);
                        }
                        LogMessage::Flush => {
                            let _ = writer.flush();
                        }
                        LogMessage::Finish => {
                            let _ = writer.finish();
                            return;
                        }
                    }
                }
                // channel closed without Finish — still finalize
                let _ = writer.flush();
            })
            .expect("Failed to spawn logging thread");

        Self {
            sender,
            handle: Some(handle),
        }
    }

    fn send(&self, msg: LogMessage) {
        let _ = self.sender.send(msg);
    }

    fn finish(&mut self) {
        self.send(LogMessage::Finish);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }
}

pub struct Logger {
    pub should_log: AtomicBool,
    level: AtomicU8,
    engine_coms: AtomicBool,
    thread: Mutex<Option<LoggingThread>>,
}

impl Default for Logger {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(target_os = "windows")]
const THREAD_ID_WIDTH: usize = 3;

#[cfg(not(target_os = "windows"))]
const THREAD_ID_WIDTH: usize = 20;

impl Logger {
    pub fn new() -> Self {
        Self {
            should_log: AtomicBool::new(false),
            level: AtomicU8::new(LogLevel::Warn as u8),
            engine_coms: AtomicBool::new(false),
            thread: Mutex::new(None),
        }
    }

    pub fn setup(
        &self,
        file_path: &str,
        level: LogLevel,
        append: bool,
        compress: bool,
        engine_coms: bool,
    ) {
        self.level.store(level as u8, Ordering::Relaxed);
        self.engine_coms.store(engine_coms, Ordering::Relaxed);

        if !file_path.is_empty() {
            let file = std::fs::OpenOptions::new()
                .create(true)
                .write(true)
                .append(append)
                .truncate(!append)
                .open(file_path);

            match file {
                Ok(f) => {
                    let writer = LogWriter::new(f, compress).expect("Failed to create log writer");
                    let logging_thread = LoggingThread::new(writer);
                    *self.thread.lock().unwrap() = Some(logging_thread);
                    self.should_log.store(true, Ordering::Relaxed);
                }
                Err(e) => {
                    eprintln!("Failed to open log file '{}': {}", file_path, e);
                }
            }
        }
    }

    pub fn print(&self, level: LogLevel, msg: &str) {
        if level >= LogLevel::Warn {
            eprintln!("{}", msg);
        } else {
            println!("{}", msg);
        }
        self.write_to_file_formatted(level, msg, false);
    }

    pub fn log(&self, level: LogLevel, msg: &str) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }
        self.write_to_file_formatted(level, msg, false);
    }

    pub fn read_from_engine(
        &self,
        line: &str,
        timestamp: &str,
        engine_name: &str,
        is_err: bool,
        thread_id: Option<u64>,
    ) {
        if !self.should_log.load(Ordering::Relaxed) || !self.engine_coms.load(Ordering::Relaxed) {
            return;
        }

        let thread_id = thread_id.unwrap_or_else(Self::get_thread_id);
        let stderr_prefix = if is_err { "<stderr> " } else { "" };
        let prefix = self.make_prefix("Engine", timestamp, thread_id);
        let formatted_msg = format!(
            "{} {} {} ---> {}\n",
            prefix, stderr_prefix, engine_name, line
        );

        self.write_raw(formatted_msg);
    }

    pub fn write_to_engine(
        &self,
        input: &str,
        timestamp: &str,
        engine_name: &str,
        thread_id: Option<u64>,
    ) {
        if !self.should_log.load(Ordering::Relaxed) || !self.engine_coms.load(Ordering::Relaxed) {
            return;
        }

        let actual_timestamp = if timestamp.is_empty() {
            get_precise_timestamp()
        } else {
            timestamp.to_string()
        };

        let thread_id = thread_id.unwrap_or_else(Self::get_thread_id);
        let prefix = self.make_prefix("Engine", &actual_timestamp, thread_id);
        let formatted_msg = format!("{}  {} <--- {}\n", prefix, engine_name, input);

        self.write_raw(formatted_msg);
    }

    fn make_prefix(&self, label: &str, timestamp: &str, thread_id: u64) -> String {
        format!(
            "[{:<6}] [{:>15}] <{:>width$}>",
            label,
            timestamp,
            thread_id,
            width = THREAD_ID_WIDTH
        )
    }

    fn write_to_file_formatted(&self, level: LogLevel, msg: &str, include_thread: bool) {
        if !self.should_log.load(Ordering::Relaxed) {
            return;
        }

        if level < LogLevel::from_u8(self.level.load(Ordering::Relaxed)) {
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
        let thread_id = if include_thread {
            Self::get_thread_id()
        } else {
            0
        };
        let prefix = self.make_prefix(label, &timestamp, thread_id);
        let formatted_msg = format!("{} fastchess --- {}\n", prefix, msg);

        self.write_raw(formatted_msg);
    }

    fn write_raw(&self, msg: String) {
        if let Some(ref t) = *self.thread.lock().unwrap() {
            t.send(LogMessage::Write(msg));
            t.send(LogMessage::Flush);
        }
    }

    fn get_thread_id() -> u64 {
        format!("{:?}", std::thread::current().id())
            .trim_start_matches("ThreadId(")
            .trim_end_matches(')')
            .parse::<u64>()
            .unwrap()
    }

    pub fn finish(&self) {
        self.should_log.store(false, Ordering::Relaxed);
        if let Some(mut t) = self.thread.lock().unwrap().take() {
            t.finish();
        }
    }
}

impl Drop for Logger {
    fn drop(&mut self) {
        if let Ok(mut thread) = self.thread.lock() {
            if let Some(mut t) = thread.take() {
                t.finish();
            }
        }
    }
}

fn get_precise_timestamp() -> String {
    let now = Local::now();
    let time_str = now.format("%H:%M:%S").to_string();
    let micros = now.timestamp_subsec_micros();
    format!("{}.{:06}", time_str, micros)
}

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
