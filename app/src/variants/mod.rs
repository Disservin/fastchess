//! Game variants module.
//!
//! This module defines the trait interface for all game variants and provides
//! a factory for creating game instances. Each variant (chess, shogi, etc.)
//! implements the `Game` trait.

use crate::game::{Color, GameStatus};
use crate::types::VariantType;

// Re-export variant implementations
pub mod chess;
pub mod shogi;

// ── Trait Definitions ────────────────────────────────────────────────────────

/// A move that can be applied to a game.
///
/// This trait abstracts over different move types (UCI, USI, etc.)
/// so that the tournament manager doesn't need to know the specifics.
pub trait GameMove: Send + Sync + std::any::Any {
    /// Returns the move in UCI/USI notation (engine protocol format).
    fn to_uci(&self) -> String;

    /// Returns the move in SAN notation (if applicable).
    fn to_san(&self) -> Option<String>;

    /// Returns the move in LAN notation (if applicable).
    fn to_lan(&self) -> Option<String>;

    /// Clone the move as a boxed trait object.
    fn clone_box(&self) -> Box<dyn GameMove>;

    /// Returns a reference to Any for downcasting.
    fn as_any(&self) -> &dyn std::any::Any;
}

impl Clone for Box<dyn GameMove> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

/// Core trait that all game variants must implement.
///
/// This trait provides a uniform interface for the tournament manager
/// to interact with any game variant without knowing the specifics.
pub trait Game: Send + Sync {
    /// Creates a boxed clone of this game.
    fn clone_box(&self) -> Box<dyn Game>;

    /// Returns the variant type.
    fn variant(&self) -> VariantType;

    /// Returns the current side to move.
    fn side_to_move(&self) -> Color;

    /// Returns the half-move clock (for fifty-move rule, etc.).
    /// Returns 0 for variants without this concept.
    fn halfmove_clock(&self) -> u32;

    /// Returns the ply count (number of half-moves played).
    fn ply_count(&self) -> u32;

    /// Returns the current game status.
    fn status(&self) -> GameStatus;

    /// Parses a move from UCI/USI notation.
    fn parse_move(&self, notation: &str) -> Option<Box<dyn GameMove>>;

    /// Applies a move to the game.
    ///
    /// Returns true if the move was legal and applied.
    fn make_move(&mut self, mv: &dyn GameMove) -> bool;

    /// Applies a move from UCI/USI notation directly.
    ///
    /// Returns true if the move was legal and applied.
    fn make_move_notation(&mut self, notation: &str) -> bool;

    /// Returns the current position in FEN/SFEN format.
    fn fen(&self) -> String;

    /// Converts a UCI/USI move to SAN notation.
    fn move_to_san(&self, notation: &str) -> Option<String>;

    /// Converts a UCI/USI move to LAN notation.
    fn move_to_lan(&self, notation: &str) -> Option<String>;

    /// Returns true if this game can use Syzygy tablebases.
    fn supports_syzygy(&self) -> bool;

    /// Returns a reference to the inner game as a chess game, if applicable.
    fn as_chess(&self) -> Option<&chess::ChessGame>;

    /// Returns a reference to the inner game as a shogi game, if applicable.
    fn as_shogi(&self) -> Option<&shogi::ShogiGame>;
}

impl Clone for Box<dyn Game> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

// ── Factory ───────────────────────────────────────────────────────────────────

/// Creates a new game instance for the given variant.
pub fn create_game(variant: VariantType) -> Box<dyn Game> {
    match variant {
        VariantType::Standard => Box::new(chess::ChessGame::new()),
        VariantType::Frc => Box::new(chess::ChessGame::with_variant(chess::Variant::Chess960)),
        VariantType::Shogi => Box::new(shogi::ShogiGame::new()),
    }
}

/// Creates a new game instance from a FEN/SFEN string.
pub fn create_game_from_fen(variant: VariantType, fen: &str) -> Option<Box<dyn Game>> {
    match variant {
        VariantType::Standard => chess::ChessGame::from_fen(fen, chess::Variant::Standard)
            .map(|g| Box::new(g) as Box<dyn Game>),
        VariantType::Frc => chess::ChessGame::from_fen(fen, chess::Variant::Chess960)
            .map(|g| Box::new(g) as Box<dyn Game>),
        VariantType::Shogi => {
            shogi::ShogiGame::from_sfen(fen).map(|g| Box::new(g) as Box<dyn Game>)
        }
    }
}

/// Returns the default starting FEN/SFEN for a variant.
pub fn default_fen(variant: VariantType) -> &'static str {
    match variant {
        VariantType::Standard | VariantType::Frc => {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        }
        VariantType::Shogi => "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
    }
}

// ── Helper Types ─────────────────────────────────────────────────────────────

/// A type-erased game instance that wraps a boxed trait object.
///
/// This is the type that the rest of the codebase should use.
#[derive(Clone)]
pub struct GameInstance {
    inner: Box<dyn Game>,
}

impl GameInstance {
    /// Creates a new game for the given variant.
    pub fn new(variant: VariantType) -> Self {
        Self {
            inner: create_game(variant),
        }
    }

    /// Creates a new game from a FEN/SFEN string.
    pub fn from_fen(fen: &str, variant: VariantType) -> Option<Self> {
        create_game_from_fen(variant, fen).map(|inner| Self { inner })
    }

    /// Returns the variant type.
    pub fn variant(&self) -> VariantType {
        self.inner.variant()
    }

    /// Returns the current side to move.
    pub fn side_to_move(&self) -> Color {
        self.inner.side_to_move()
    }

    /// Returns the half-move clock.
    pub fn halfmove_clock(&self) -> u32 {
        self.inner.halfmove_clock()
    }

    /// Returns the ply count.
    pub fn ply_count(&self) -> u32 {
        self.inner.ply_count()
    }

    /// Returns the current game status.
    pub fn status(&self) -> GameStatus {
        self.inner.status()
    }

    /// Parses a move from notation.
    pub fn parse_move(&self, notation: &str) -> Option<Box<dyn GameMove>> {
        self.inner.parse_move(notation)
    }

    /// Makes a move on the board.
    pub fn make_move(&mut self, mv: &dyn GameMove) -> bool {
        self.inner.make_move(mv)
    }

    /// Makes a move from notation.
    pub fn make_move_notation(&mut self, notation: &str) -> bool {
        self.inner.make_move_notation(notation)
    }

    /// Returns the current FEN/SFEN.
    pub fn fen(&self) -> String {
        self.inner.fen()
    }

    /// Converts a move to SAN.
    pub fn move_to_san(&self, notation: &str) -> Option<String> {
        self.inner.move_to_san(notation)
    }

    /// Converts a move to LAN.
    pub fn move_to_lan(&self, notation: &str) -> Option<String> {
        self.inner.move_to_lan(notation)
    }

    /// Returns true if Syzygy is supported.
    pub fn supports_syzygy(&self) -> bool {
        self.inner.supports_syzygy()
    }

    /// Returns a reference to the inner chess game.
    pub fn as_chess(&self) -> Option<&chess::ChessGame> {
        self.inner.as_chess()
    }

    /// Returns a reference to the inner shogi game.
    pub fn as_shogi(&self) -> Option<&shogi::ShogiGame> {
        self.inner.as_shogi()
    }

    /// Makes a move from UCI/USI notation (convenience method).
    pub fn make_uci_move(&mut self, notation: &str) -> bool {
        self.inner.make_move_notation(notation)
    }

    /// Returns the default starting FEN/SFEN for a variant.
    pub fn default_fen(variant: VariantType) -> &'static str {
        default_fen(variant)
    }
}

impl std::fmt::Debug for GameInstance {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GameInstance")
            .field("variant", &self.variant())
            .field("fen", &self.fen())
            .finish()
    }
}
