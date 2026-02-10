//! Shared game types and utilities.
//!
//! This module contains types that are shared across all game variants,
//! such as Color, GameStatus, etc. The actual game implementations
//! are in the `variants` module.

use crate::types::VariantType;

// Re-export variant-related items
pub use crate::variants::GameInstance;

// ── Shared Types ─────────────────────────────────────────────────────────────

/// Unified color type for all game variants.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Color {
    /// First player (White in chess, Sente in shogi)
    First,
    /// Second player (Black in chess, Gote in shogi)
    Second,
}

impl Color {
    /// Returns the opposite color.
    pub fn opposite(self) -> Self {
        match self {
            Color::First => Color::Second,
            Color::Second => Color::First,
        }
    }

    /// Returns a human-readable name for this color in context.
    pub fn name(self, variant: VariantType) -> &'static str {
        match (self, variant) {
            (Color::First, VariantType::Shogi) => "Sente",
            (Color::Second, VariantType::Shogi) => "Gote",
            (Color::First, _) => "White",
            (Color::Second, _) => "Black",
        }
    }
}

/// Reason the game ended.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GameOverReason {
    /// Game is not over.
    None,
    /// Checkmate (chess) or tsumi (shogi).
    Checkmate,
    /// Stalemate (chess only - no legal moves, not in check).
    Stalemate,
    /// Insufficient material to mate (chess only).
    InsufficientMaterial,
    /// Threefold repetition (chess) or fourfold/sennichite (shogi).
    Repetition,
    /// Fifty-move rule (chess only).
    FiftyMoveRule,
    /// Perpetual check loss (shogi only).
    PerpetualCheck,
    /// Illegal move (shogi impasse declaration failure, etc.)
    IllegalMove,
    /// Win by impasse declaration (shogi only).
    ImpasseWin,
}

impl GameOverReason {
    /// Returns a human-readable message for this game-over reason.
    pub fn message(self, winner: Option<Color>, variant: VariantType) -> String {
        match self {
            GameOverReason::None => String::new(),
            GameOverReason::Checkmate => match winner {
                Some(c) => format!("{} mates", c.name(variant)),
                None => "Checkmate".to_string(),
            },
            GameOverReason::Stalemate => "Draw by stalemate".to_string(),
            GameOverReason::InsufficientMaterial => {
                "Draw by insufficient mating material".to_string()
            }
            GameOverReason::Repetition => match variant {
                VariantType::Shogi => "Draw by 4-fold repetition".to_string(),
                _ => "Draw by 3-fold repetition".to_string(),
            },
            GameOverReason::FiftyMoveRule => "Draw by fifty moves rule".to_string(),
            GameOverReason::PerpetualCheck => match winner {
                Some(c) => format!("{} wins by perpetual check", c.name(variant)),
                None => "Loss by perpetual check".to_string(),
            },
            GameOverReason::IllegalMove => match winner {
                Some(c) => format!("{} wins by illegal move", c.name(variant)),
                None => "Loss by illegal move".to_string(),
            },
            GameOverReason::ImpasseWin => match winner {
                Some(c) => format!("{} wins by impasse", c.name(variant)),
                None => "Win by impasse".to_string(),
            },
        }
    }

    /// Returns true if this reason represents a draw.
    pub fn is_draw(self) -> bool {
        matches!(
            self,
            GameOverReason::Stalemate
                | GameOverReason::InsufficientMaterial
                | GameOverReason::Repetition
                | GameOverReason::FiftyMoveRule
        )
    }
}

/// Result of checking if the game is over.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GameStatus {
    /// The reason the game ended, or None if ongoing.
    pub reason: GameOverReason,
    /// The winner, if any (None for draws or ongoing games).
    pub winner: Option<Color>,
}

impl GameStatus {
    /// Game is still ongoing.
    pub const ONGOING: Self = Self {
        reason: GameOverReason::None,
        winner: None,
    };

    /// Returns true if the game is over.
    pub fn is_game_over(&self) -> bool {
        self.reason != GameOverReason::None
    }

    /// Returns true if the game ended in a draw.
    pub fn is_draw(&self) -> bool {
        self.reason.is_draw()
    }

    /// Returns true if the game is still ongoing.
    pub fn is_ongoing(&self) -> bool {
        self.reason == GameOverReason::None
    }
}

// ── Submodules ───────────────────────────────────────────────────────────────

pub mod book;
pub mod pgn;
pub mod syzygy;
pub mod timecontrol;
