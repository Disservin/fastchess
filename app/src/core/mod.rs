pub mod config;
pub mod file_writer;
pub mod logger;

use chrono::Local;
use std::time::{SystemTime, UNIX_EPOCH};

/// Get current datetime as ISO 8601 string.
pub fn datetime_iso() -> String {
    Local::now().to_rfc3339()
}

/// Get current datetime with a custom format string.
pub fn datetime(fmt: &str) -> Option<String> {
    Some(Local::now().format(fmt).to_string())
}

/// Get high-precision datetime for log timestamps (HH:MM:SS.microseconds format).
pub fn datetime_precise() -> String {
    let now = Local::now();
    let time_str = now.format("%H:%M:%S").to_string();
    let micros = now.timestamp_subsec_micros();
    format!("{}.{:06}", time_str, micros)
}

/// Format a duration as "H:MM:SS".
pub fn duration_string(dur: std::time::Duration) -> String {
    let total_secs = dur.as_secs();
    let hours = total_secs / 3600;
    let minutes = (total_secs % 3600) / 60;
    let seconds = total_secs % 60;
    format!("{}:{:02}:{:02}", hours, minutes, seconds)
}

fn datetime_fast(format: &str) -> String {
    let now = SystemTime::now();
    let dur = now.duration_since(UNIX_EPOCH).unwrap();
    let secs = dur.as_secs() as libc::time_t;

    #[cfg(windows)]
    let tm_buf = unsafe {
        let mut tm_buf = std::mem::zeroed::<libc::tm>();
        libc::localtime_s(&mut tm_buf, &secs);
        tm_buf
    };

    #[cfg(not(windows))]
    let tm_buf = unsafe {
        let mut tm_buf = std::mem::zeroed::<libc::tm>();
        libc::localtime_r(&secs, &mut tm_buf);
        tm_buf
    };

    let hour = tm_buf.tm_hour;
    let min = tm_buf.tm_min;
    let sec = tm_buf.tm_sec;

    format
        .replace("%H", &format!("{:02}", hour))
        .replace("%M", &format!("{:02}", min))
        .replace("%S", &format!("{:02}", sec))
        .replace("%Y", &format!("{:04}", 1900 + tm_buf.tm_year))
        .replace("%m", &format!("{:02}", tm_buf.tm_mon + 1))
        .replace("%d", &format!("{:02}", tm_buf.tm_mday))
}

pub fn datetime_precise_fast() -> String {
    let now = SystemTime::now();
    let dur = now.duration_since(UNIX_EPOCH).unwrap();
    let micros = dur.subsec_micros();
    let time_str = datetime_fast("%H:%M:%S");
    format!("{}.{:06}", time_str, micros)
}

pub mod str_utils {
    /// Find the element *after* a keyword in a token list and parse it.
    ///
    /// Given tokens `["info", "depth", "20", "score", "cp", "15"]`,
    /// `find_element::<i64>(&tokens, "depth")` returns `Some(20)`.
    pub fn find_element<T: std::str::FromStr>(tokens: &[&str], keyword: &str) -> Option<T> {
        let pos = tokens.iter().position(|s| *s == keyword)?;
        let next = tokens.get(pos + 1)?;
        next.parse().ok()
    }

    /// Find the element after a keyword, returning a Result with error context.
    pub fn find_element_result<T: std::str::FromStr>(
        tokens: &[&str],
        keyword: &str,
    ) -> Result<T, String> {
        let pos = tokens
            .iter()
            .position(|s| *s == keyword)
            .ok_or_else(|| format!("Keyword '{}' not found", keyword))?;
        let next = tokens
            .get(pos + 1)
            .ok_or_else(|| format!("No value after keyword '{}'", keyword))?;
        next.parse()
            .map_err(|_| format!("Failed to parse value after '{}'", keyword))
    }
}
