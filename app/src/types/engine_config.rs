use serde::{Deserialize, Serialize};

use super::enums::*;
use crate::game::timecontrol::TimeControlLimits;

/// Pair of white/black items (engines, configs, player info, etc.)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GamePair<W, B = W> {
    pub white: W,
    pub black: B,
}

impl<W, B> GamePair<W, B> {
    pub fn new(white: W, black: B) -> Self {
        Self { white, black }
    }
}

/// Engine resource limits for the `go` command.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Limit {
    pub tc: TimeControlLimits,
    pub nodes: u64,
    pub plies: u64,
}

/// Configuration for a single chess engine.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EngineConfiguration {
    /// The limit for the engine's `go` command.
    pub limit: Limit,
    /// Engine display name.
    pub name: String,
    /// Path to engine working directory.
    pub dir: String,
    /// Engine binary name / command.
    pub cmd: String,
    /// Custom arguments to pass on the command line.
    pub args: String,
    /// Whether to restart the engine process after every game.
    pub restart: bool,
    /// UCI options as (name, value) pairs.
    pub options: Vec<(String, String)>,
    /// Chess variant.
    pub variant: VariantType,
}

impl Default for EngineConfiguration {
    fn default() -> Self {
        Self {
            limit: Limit::default(),
            name: String::new(),
            dir: String::new(),
            cmd: String::new(),
            args: String::new(),
            restart: false,
            options: Vec::new(),
            variant: VariantType::Standard,
        }
    }
}

impl EngineConfiguration {
    /// Look up a UCI option by name and transform its value.
    pub fn get_option<T, F>(&self, option_name: &str, transform: F) -> Option<T>
    where
        F: Fn(&str) -> T,
    {
        self.options
            .iter()
            .find(|(name, _)| name == option_name)
            .map(|(_, value)| transform(value))
    }
}
