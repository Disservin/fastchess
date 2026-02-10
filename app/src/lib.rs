pub mod affinity;
pub mod cli;
pub mod core;
pub mod engine;
pub mod game;
pub mod matchmaking;
pub mod types;
pub mod variants;

use std::sync::atomic::AtomicBool;

/// Global stop flag - signals all components to shut down gracefully.
pub static STOP: AtomicBool = AtomicBool::new(false);
/// Set when the program terminates abnormally (engine crash without recover, etc.)
pub static ABNORMAL_TERMINATION: AtomicBool = AtomicBool::new(false);
