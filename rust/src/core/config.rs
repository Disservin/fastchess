//! Global tournament configuration.
//!
//! Ports `core/config/config.hpp` from C++.
//!
//! The C++ code uses `inline std::unique_ptr<Tournament>` globals. In Rust we
//! use `OnceLock` for safe, single-initialization global state.

use std::sync::OnceLock;

use crate::types::engine_config::EngineConfiguration;
use crate::types::tournament::TournamentConfig;

/// Global tournament configuration, set once at startup.
pub static TOURNAMENT_CONFIG: OnceLock<TournamentConfig> = OnceLock::new();

/// Global engine configurations, set once at startup.
pub static ENGINE_CONFIGS: OnceLock<Vec<EngineConfiguration>> = OnceLock::new();

/// Initialize the global tournament config. Panics if already initialized.
pub fn set_tournament_config(config: TournamentConfig) {
    TOURNAMENT_CONFIG
        .set(config)
        .expect("Tournament config already initialized");
}

/// Initialize the global engine configs. Panics if already initialized.
pub fn set_engine_configs(configs: Vec<EngineConfiguration>) {
    ENGINE_CONFIGS
        .set(configs)
        .expect("Engine configs already initialized");
}

/// Get a reference to the global tournament config.
///
/// Panics if not yet initialized (call `set_tournament_config` first).
pub fn tournament_config() -> &'static TournamentConfig {
    TOURNAMENT_CONFIG
        .get()
        .expect("Tournament config not initialized")
}

/// Get a reference to the global engine configs.
///
/// Panics if not yet initialized (call `set_engine_configs` first).
pub fn engine_configs() -> &'static [EngineConfiguration] {
    ENGINE_CONFIGS
        .get()
        .expect("Engine configs not initialized")
}
