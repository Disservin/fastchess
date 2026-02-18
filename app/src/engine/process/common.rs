use std::io;

use thiserror::Error;

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

#[derive(Error, Debug, Clone)]
pub enum ProcessError {
    #[error("failed to initialize interruption descriptors: {0}")]
    Io(String),

    #[error("{0}")]
    General(String),

    #[error("timeout")]
    Timeout,
}

impl From<io::Error> for ProcessError {
    fn from(e: io::Error) -> Self {
        Self::Io(e.to_string())
    }
}

impl From<&str> for ProcessError {
    fn from(s: &str) -> Self {
        Self::General(s.to_string())
    }
}

impl From<String> for ProcessError {
    fn from(s: String) -> Self {
        Self::General(s)
    }
}

/// Result type for process operations.
pub type ProcessResult<T = ()> = Result<T, ProcessError>;

/// Extension trait for ProcessResult to provide helper methods.
pub trait ProcessResultExt {
    fn is_timeout(&self) -> bool;
}

impl<T> ProcessResultExt for ProcessResult<T> {
    fn is_timeout(&self) -> bool {
        matches!(self, Err(ProcessError::Timeout))
    }
}

/// A line of output from an engine process.
#[derive(Debug, Clone)]
pub struct Line {
    pub line: String,
    pub time: String,
    pub std: Standard,
}
