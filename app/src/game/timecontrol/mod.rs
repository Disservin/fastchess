use serde::{Deserialize, Serialize};
use std::fmt;
use std::time::Duration;

/// Raw time control limits as specified on the command line.
/// All time values are in milliseconds.
#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct TimeControlLimits {
    /// Increment per move (winc/binc), in milliseconds.
    pub increment: i64,
    /// Fixed time per move (movetime), in milliseconds.
    pub fixed_time: i64,
    /// Total time (wtime/btime), in milliseconds.
    pub time: i64,
    /// Moves per time control period (movestogo).
    pub moves: i64,
    /// Time margin before declaring a timeout, in milliseconds.
    pub timemargin: i64,
}

/// Manages the live time control state during a game.
///
/// Created from `TimeControlLimits`; tracks remaining time and moves.
#[derive(Debug, Clone)]
pub struct TimeControl {
    limits: TimeControlLimits,
    time_left: i64,
    moves_left: i64,
}

/// Default margin (ms) added to timeout threshold for safety.
const MARGIN: i64 = 100;

impl TimeControl {
    pub fn new(limits: &TimeControlLimits) -> Self {
        let time_left = if limits.fixed_time != 0 {
            limits.fixed_time
        } else {
            limits.time + limits.increment
        };

        Self {
            limits: limits.clone(),
            time_left,
            moves_left: limits.moves,
        }
    }

    /// The maximum time to wait for engine output before declaring a timeout.
    pub fn get_timeout_threshold(&self) -> Duration {
        let ms = self.time_left + self.limits.timemargin + MARGIN;
        Duration::from_millis(ms.max(0) as u64)
    }

    /// Subtract elapsed time from the clock. Returns `false` if time is exceeded.
    pub fn update_time(&mut self, elapsed_millis: i64) -> bool {
        if self.limits.moves > 0 {
            if self.moves_left == 1 {
                self.moves_left = self.limits.moves;
                self.time_left += self.limits.time;
            } else {
                self.moves_left -= 1;
            }
        }

        // No time control (e.g. nodes-only or depth-only)
        if self.limits.fixed_time == 0 && self.limits.time + self.limits.increment == 0 {
            return true;
        }

        self.time_left -= elapsed_millis;

        if self.time_left < -self.limits.timemargin {
            return false;
        }

        if self.time_left < 0 {
            self.time_left = 0;
        }

        self.time_left += self.limits.increment;

        if self.limits.fixed_time != 0 {
            self.time_left = self.limits.fixed_time;
        }

        true
    }

    pub fn get_time_left(&self) -> i64 {
        self.time_left
    }
    pub fn get_moves_left(&self) -> i64 {
        self.moves_left
    }
    pub fn get_fixed_time(&self) -> i64 {
        self.limits.fixed_time
    }
    pub fn get_increment(&self) -> i64 {
        self.limits.increment
    }

    pub fn is_fixed_time(&self) -> bool {
        self.limits.fixed_time != 0
    }
    pub fn is_timed(&self) -> bool {
        self.limits.time != 0
    }
    pub fn is_moves(&self) -> bool {
        self.limits.moves != 0
    }
    pub fn is_increment(&self) -> bool {
        self.limits.increment != 0
    }

    /// Whether any meaningful time control is configured.
    pub fn has_time_control(&self) -> bool {
        self.limits.time + self.limits.increment > 0 || self.limits.fixed_time > 0
    }
}

impl fmt::Display for TimeControl {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.limits.fixed_time > 0 {
            return write!(f, "{}/move", self.limits.fixed_time as f64 / 1000.0);
        }

        if self.limits.moves == 0 && self.limits.time == 0 && self.limits.increment == 0 {
            write!(f, "-")?;
            return Ok(());
        }

        if self.limits.moves > 0 {
            write!(f, "{}/", self.limits.moves)?;
        }

        if self.limits.time + self.limits.increment > 0 {
            write!(f, "{}", self.limits.time as f64 / 1000.0)?;
        }

        if self.limits.increment > 0 {
            write!(f, "+{}", self.limits.increment as f64 / 1000.0)?;
        }

        Ok(())
    }
}
