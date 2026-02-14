//! Cross-platform process management for engine communication.
//!
//! This module provides a unified interface for spawning and communicating with
//! chess engine processes across Unix and Windows platforms.

// Platform-independent types and traits
mod common;
pub use common::{Line, ProcessResult, Standard, Status};

// Platform-specific implementations
#[cfg(unix)]
mod unix;
#[cfg(unix)]
pub use unix::Process;

#[cfg(windows)]
mod windows;
#[cfg(windows)]
pub use windows::Process;
