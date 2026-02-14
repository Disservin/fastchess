//! Common types and traits for process management.

use std::time::Duration;

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
