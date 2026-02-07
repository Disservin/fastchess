//! Adjudication trackers for draw, resign, max-moves, and tablebase conditions.
//!
//! Ports the `DrawTracker`, `ResignTracker`, `MaxMovesTracker`, and
//! `TbAdjudicationTracker` classes from `matchmaking/match/match.hpp`.

use crate::engine::uci_engine::{Color, Score, ScoreType};
use crate::types::adjudication::*;

// ── Draw Tracker ─────────────────────────────────────────────────────────────

/// Tracks whether the game should be adjudicated as a draw.
///
/// A draw is declared when both engines report scores within `draw_score`
/// centipawns for `move_count * 2` consecutive plies (i.e. `move_count` full
/// moves from each side), and the ply count has reached `move_number`.
#[derive(Debug, Clone)]
pub struct DrawTracker {
    draw_moves: i32,
    draw_score: i32,
    move_number: u32,
    move_count: i32,
}

impl DrawTracker {
    pub fn new(adj: &DrawAdjudication) -> Self {
        Self {
            draw_moves: 0,
            draw_score: adj.score,
            move_number: adj.move_number,
            move_count: adj.move_count,
        }
    }

    /// Update tracker with the engine's reported score and the half-move clock.
    pub fn update(&mut self, score: &Score, hmvc: i32) {
        if hmvc == 0 {
            self.draw_moves = 0;
        }

        if self.move_count > 0 {
            if score.value.unsigned_abs() <= self.draw_score as u64
                && score.score_type == ScoreType::Cp
            {
                self.draw_moves += 1;
            } else {
                self.draw_moves = 0;
            }
        }
    }

    /// Check if a draw can be adjudicated at the given ply count.
    pub fn adjudicatable(&self, plies: u32) -> bool {
        plies >= self.move_number && self.draw_moves >= self.move_count * 2
    }

    /// Reset the consecutive draw-move counter.
    pub fn invalidate(&mut self) {
        self.draw_moves = 0;
    }
}

// ── Resign Tracker ───────────────────────────────────────────────────────────

/// Tracks whether a resignation adjudication should occur.
///
/// In twosided mode, both engines must agree the position is lost/won.
/// In one-sided mode, each engine's resignation is tracked independently.
#[derive(Debug, Clone)]
pub struct ResignTracker {
    resign_moves: i32,
    resign_moves_black: i32,
    resign_moves_white: i32,
    resign_score: i32,
    move_count: i32,
    twosided: bool,
}

impl ResignTracker {
    pub fn new(adj: &ResignAdjudication) -> Self {
        Self {
            resign_moves: 0,
            resign_moves_black: 0,
            resign_moves_white: 0,
            resign_score: adj.score,
            move_count: adj.move_count,
            twosided: adj.twosided,
        }
    }

    /// Update tracker with the engine's score and the side that just moved.
    pub fn update(&mut self, score: &Score, color: Color) {
        if self.twosided {
            if (score.value.unsigned_abs() >= self.resign_score as u64
                && score.score_type == ScoreType::Cp)
                || score.score_type == ScoreType::Mate
            {
                self.resign_moves += 1;
            } else {
                self.resign_moves = 0;
            }
        } else {
            let counter = match color {
                Color::Black => &mut self.resign_moves_black,
                Color::White => &mut self.resign_moves_white,
            };
            if (score.value <= -(self.resign_score as i64) && score.score_type == ScoreType::Cp)
                || (score.value < 0 && score.score_type == ScoreType::Mate)
            {
                *counter += 1;
            } else {
                *counter = 0;
            }
        }
    }

    /// Returns true if resignation criteria are met.
    pub fn resignable(&self) -> bool {
        if self.twosided {
            self.resign_moves >= self.move_count * 2
        } else {
            self.resign_moves_black >= self.move_count || self.resign_moves_white >= self.move_count
        }
    }

    /// Invalidate (reset) the tracker for the given color.
    pub fn invalidate(&mut self, color: Color) {
        if self.twosided {
            self.resign_moves = 0;
            return;
        }
        match color {
            Color::Black => self.resign_moves_black = 0,
            Color::White => self.resign_moves_white = 0,
        }
    }
}

// ── Max Moves Tracker ────────────────────────────────────────────────────────

/// Tracks the number of plies (half-moves) played and adjudicates a draw
/// when `move_count * 2` plies have been reached.
#[derive(Debug, Clone)]
pub struct MaxMovesTracker {
    max_moves: i32,
    move_count: i32,
}

impl MaxMovesTracker {
    pub fn new(adj: &MaxMovesAdjudication) -> Self {
        Self {
            max_moves: 0,
            move_count: adj.move_count,
        }
    }

    pub fn update(&mut self) {
        self.max_moves += 1;
    }

    pub fn max_moves_reached(&self) -> bool {
        self.max_moves >= self.move_count * 2
    }
}

// ── TB Adjudication Tracker ──────────────────────────────────────────────────

/// Placeholder for Syzygy tablebase adjudication.
///
/// The full implementation requires a Syzygy probing library. For now this
/// tracks the configuration but the actual probing methods are stubbed.
#[derive(Debug, Clone)]
pub struct TbAdjudicationTracker {
    #[allow(dead_code)]
    max_pieces: i32,
    #[allow(dead_code)]
    ignore_50_move_rule: bool,
}

impl TbAdjudicationTracker {
    pub fn new(adj: &TbAdjudication) -> Self {
        Self {
            max_pieces: adj.max_pieces,
            ignore_50_move_rule: adj.ignore_50_move_rule,
        }
    }

    /// Check if the position can be probed in tablebases.
    ///
    /// TODO: Implement with actual Syzygy probing. Currently always returns false.
    pub fn adjudicatable(&self, _piece_count: u32) -> bool {
        // Would check: max_pieces != 0 && piece_count > max_pieces => false
        // Then call canProbeSyzygyWdl(board)
        false
    }

    /// Probe the tablebase for the game result.
    ///
    /// TODO: Implement with actual Syzygy probing.
    pub fn adjudicate(&self) -> crate::types::match_data::GameResult {
        crate::types::match_data::GameResult::None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_draw_tracker_basic() {
        let adj = DrawAdjudication {
            move_number: 0,
            move_count: 2,
            score: 10,
            enabled: true,
        };
        let mut tracker = DrawTracker::new(&adj);

        let score = Score {
            score_type: ScoreType::Cp,
            value: 5,
        };

        // Need 2 * 2 = 4 consecutive plies below threshold
        assert!(!tracker.adjudicatable(0));
        tracker.update(&score, 1);
        assert!(!tracker.adjudicatable(1));
        tracker.update(&score, 2);
        assert!(!tracker.adjudicatable(2));
        tracker.update(&score, 3);
        assert!(!tracker.adjudicatable(3));
        tracker.update(&score, 4);
        assert!(tracker.adjudicatable(4));
    }

    #[test]
    fn test_draw_tracker_reset_on_hmvc_zero() {
        let adj = DrawAdjudication {
            move_number: 0,
            move_count: 1,
            score: 10,
            enabled: true,
        };
        let mut tracker = DrawTracker::new(&adj);

        let score = Score {
            score_type: ScoreType::Cp,
            value: 5,
        };

        tracker.update(&score, 1);
        assert!(!tracker.adjudicatable(0));
        // hmvc=0 resets
        tracker.update(&score, 0);
        assert!(!tracker.adjudicatable(0));
    }

    #[test]
    fn test_draw_tracker_score_too_high() {
        let adj = DrawAdjudication {
            move_number: 0,
            move_count: 1,
            score: 10,
            enabled: true,
        };
        let mut tracker = DrawTracker::new(&adj);

        let high_score = Score {
            score_type: ScoreType::Cp,
            value: 50,
        };
        tracker.update(&high_score, 1);
        tracker.update(&high_score, 2);
        assert!(!tracker.adjudicatable(2));
    }

    #[test]
    fn test_draw_tracker_move_number_threshold() {
        let adj = DrawAdjudication {
            move_number: 40, // only adjudicate after ply 40
            move_count: 1,
            score: 10,
            enabled: true,
        };
        let mut tracker = DrawTracker::new(&adj);
        let score = Score {
            score_type: ScoreType::Cp,
            value: 5,
        };
        tracker.update(&score, 1);
        tracker.update(&score, 2);
        // Enough consecutive moves, but ply < 40
        assert!(!tracker.adjudicatable(30));
        assert!(tracker.adjudicatable(40));
    }

    #[test]
    fn test_resign_tracker_twosided() {
        let adj = ResignAdjudication {
            move_count: 2,
            score: 500,
            twosided: true,
            enabled: true,
        };
        let mut tracker = ResignTracker::new(&adj);

        let losing = Score {
            score_type: ScoreType::Cp,
            value: -600,
        };

        // Need 2 * 2 = 4 consecutive plies with |score| >= 500
        tracker.update(&losing, Color::White);
        assert!(!tracker.resignable());
        tracker.update(&losing, Color::Black);
        assert!(!tracker.resignable());
        tracker.update(&losing, Color::White);
        assert!(!tracker.resignable());
        tracker.update(&losing, Color::Black);
        assert!(tracker.resignable());
    }

    #[test]
    fn test_resign_tracker_onesided() {
        let adj = ResignAdjudication {
            move_count: 2,
            score: 500,
            twosided: false,
            enabled: true,
        };
        let mut tracker = ResignTracker::new(&adj);

        let losing = Score {
            score_type: ScoreType::Cp,
            value: -600,
        };

        // One-sided: black reports losing twice
        tracker.update(&losing, Color::Black);
        assert!(!tracker.resignable());
        tracker.update(&losing, Color::Black);
        assert!(tracker.resignable());
    }

    #[test]
    fn test_resign_tracker_mate_twosided() {
        let adj = ResignAdjudication {
            move_count: 1,
            score: 500,
            twosided: true,
            enabled: true,
        };
        let mut tracker = ResignTracker::new(&adj);

        let mate = Score {
            score_type: ScoreType::Mate,
            value: -3,
        };

        tracker.update(&mate, Color::White);
        assert!(!tracker.resignable());
        tracker.update(&mate, Color::Black);
        assert!(tracker.resignable());
    }

    #[test]
    fn test_resign_tracker_invalidate() {
        let adj = ResignAdjudication {
            move_count: 1,
            score: 500,
            twosided: false,
            enabled: true,
        };
        let mut tracker = ResignTracker::new(&adj);

        let losing = Score {
            score_type: ScoreType::Cp,
            value: -600,
        };

        tracker.update(&losing, Color::Black);
        tracker.invalidate(Color::Black);
        assert!(!tracker.resignable());
    }

    #[test]
    fn test_maxmoves_tracker() {
        let adj = MaxMovesAdjudication {
            move_count: 3,
            enabled: true,
        };
        let mut tracker = MaxMovesTracker::new(&adj);

        // 3 moves = 6 plies
        for _ in 0..5 {
            tracker.update();
            assert!(!tracker.max_moves_reached());
        }
        tracker.update();
        assert!(tracker.max_moves_reached());
    }
}
