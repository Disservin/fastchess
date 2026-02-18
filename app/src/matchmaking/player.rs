//! Player wrapper around a UCI engine and its time control.
//!
//! Ports `matchmaking/player.hpp` from C++.

use std::time::Duration;

use crate::engine::uci_engine::UciEngine;
use crate::game::timecontrol::TimeControl;
use crate::types::match_data::GameResult;

/// A player in a match: wraps a UCI engine reference with its time control
/// state and game result.
pub struct Player<'a> {
    pub engine: &'a mut UciEngine,
    time_control: TimeControl,
    result: GameResult,
}

impl<'a> Player<'a> {
    pub fn new(engine: &'a mut UciEngine) -> Self {
        let time_control = TimeControl::new(&engine.config().limit.tc);
        Self {
            engine,
            time_control,
            result: GameResult::None,
        }
    }

    /// The maximum time to wait for engine output before declaring a timeout.
    /// Returns `Duration::ZERO` for nodes/depth-limited engines (no timeout).
    pub fn get_timeout_threshold(&self) -> Duration {
        let config = self.engine.config();
        if config.limit.nodes != 0 || config.limit.plies != 0 {
            return Duration::ZERO;
        }
        self.time_control.get_timeout_threshold()
    }

    pub fn time_control(&self) -> &TimeControl {
        &self.time_control
    }

    pub fn time_control_mut(&mut self) -> &mut TimeControl {
        &mut self.time_control
    }

    pub fn has_time_control(&self) -> bool {
        self.time_control.is_timed() || self.time_control.is_fixed_time()
    }

    pub fn set_lost(&mut self) {
        self.result = GameResult::Lose;
    }

    pub fn set_draw(&mut self) {
        self.result = GameResult::Draw;
    }

    pub fn set_won(&mut self) {
        self.result = GameResult::Win;
    }

    pub fn result(&self) -> GameResult {
        self.result
    }
}
