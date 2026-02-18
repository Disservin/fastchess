pub mod config;
pub mod file_writer;
pub mod logger;

use chrono::Local;

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

/// String utility functions (replaces C++ str_utils namespace).
pub mod str_utils {
    /// Split a string by a delimiter character.
    pub fn split_string(s: &str, delim: char) -> Vec<String> {
        s.split(delim).map(|s| s.to_string()).collect()
    }

    /// Join a slice of strings with a separator.
    pub fn join(parts: &[String], sep: &str) -> String {
        parts.join(sep)
    }

    /// Check if a collection of strings contains a given string.
    pub fn contains(items: &[String], target: &str) -> bool {
        items.iter().any(|s| s == target)
    }

    /// Find the element *after* a keyword in a token list and parse it.
    ///
    /// Given tokens `["info", "depth", "20", "score", "cp", "15"]`,
    /// `find_element::<i64>(&tokens, "depth")` returns `Some(20)`.
    pub fn find_element<T: std::str::FromStr>(tokens: &[String], keyword: &str) -> Option<T> {
        let pos = tokens.iter().position(|s| s == keyword)?;
        let next = tokens.get(pos + 1)?;
        next.parse().ok()
    }

    /// Find the element after a keyword, returning a Result with error context.
    pub fn find_element_result<T: std::str::FromStr>(
        tokens: &[String],
        keyword: &str,
    ) -> Result<T, String> {
        let pos = tokens
            .iter()
            .position(|s| s == keyword)
            .ok_or_else(|| format!("Keyword '{}' not found", keyword))?;
        let next = tokens
            .get(pos + 1)
            .ok_or_else(|| format!("No value after keyword '{}'", keyword))?;
        next.parse()
            .map_err(|_| format!("Failed to parse value after '{}'", keyword))
    }
}
