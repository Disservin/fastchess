//! Chess variant implementation.
//!
//! Implements the `Game` trait for standard chess and Chess960.

use shakmaty::fen::Fen;
use shakmaty::san::San;
use shakmaty::uci::UciMove;
use shakmaty::zobrist::{Zobrist64, ZobristHash};
use shakmaty::{CastlingMode, Chess, Color as ChessColor, EnPassantMode, Move, Position, Role};

use crate::game::{Color, GameOverReason, GameStatus};
use crate::types::VariantType;
use crate::variants::{Game, GameMove};

// Re-export types that other modules need
pub use crate::variants::shogi::ShogiGame;

/// A validated chess move.
#[derive(Clone)]
pub struct ChessMove {
    inner: Move,
    variant: VariantType,
}

fn to_castling_mode(variant: VariantType) -> CastlingMode {
    match variant {
        VariantType::Standard => CastlingMode::Standard,
        VariantType::Frc => CastlingMode::Chess960,
        _ => CastlingMode::Standard,
    }
}

impl ChessMove {
    /// Returns the UCI representation of this move.
    pub fn to_uci(&self) -> String {
        UciMove::from_move(&self.inner, to_castling_mode(self.variant)).to_string()
    }

    /// Returns the internal shakmaty move.
    pub fn inner(&self) -> &Move {
        &self.inner
    }
}

impl GameMove for ChessMove {
    fn to_uci(&self) -> String {
        self.to_uci()
    }

    fn to_san(&self, game: &dyn crate::variants::Game) -> Option<String> {
        // Use the game's position to convert to SAN
        if let Some(chess_game) = game.as_chess() {
            use shakmaty::san::San;
            let san = San::from_move(chess_game.inner(), &self.inner);
            Some(san.to_string())
        } else {
            None
        }
    }

    fn to_lan(&self, _game: &dyn crate::variants::Game) -> Option<String> {
        // LAN doesn't need position context
        Some(move_to_lan(&self.inner))
    }

    fn clone_box(&self) -> Box<dyn GameMove> {
        Box::new(self.clone())
    }

    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}

// ── Chess Game ───────────────────────────────────────────────────────────────

/// A chess game with position tracking and automatic threefold repetition detection.
#[derive(Clone)]
pub struct ChessGame {
    board: Chess,
    variant: VariantType,
    hash_history: Vec<u64>,
    ply_count: u32,
}

impl Default for ChessGame {
    fn default() -> Self {
        Self::new()
    }
}

impl ChessGame {
    /// Creates a new game from the standard starting position.
    pub fn new() -> Self {
        let board = Chess::default();
        let initial_hash: Zobrist64 = board.zobrist_hash(EnPassantMode::Legal);

        Self {
            board,
            variant: VariantType::Standard,
            hash_history: vec![initial_hash.0],
            ply_count: 0,
        }
    }

    /// Creates a new game from a FEN string.
    pub fn from_fen(fen: &str, variant: VariantType) -> Option<Self> {
        let parsed: Fen = fen.parse().ok()?;
        let board: Chess = parsed.into_position(to_castling_mode(variant)).ok()?;
        let initial_hash: Zobrist64 = board.zobrist_hash(EnPassantMode::Legal);

        Some(Self {
            board,
            variant,
            hash_history: vec![initial_hash.0],
            ply_count: 0,
        })
    }

    /// Creates a new game with the given variant.
    pub fn with_variant(variant: VariantType) -> Self {
        let board = Chess::default();
        let initial_hash: Zobrist64 = board.zobrist_hash(EnPassantMode::Legal);

        Self {
            board,
            variant,
            hash_history: vec![initial_hash.0],
            ply_count: 0,
        }
    }

    /// Returns the variant.
    pub fn variant_type(&self) -> VariantType {
        self.variant
    }

    /// Returns a reference to the underlying shakmaty position.
    pub fn inner(&self) -> &Chess {
        &self.board
    }

    /// Checks if the position is in check.
    pub fn is_check(&self) -> bool {
        self.board.is_check()
    }

    /// Checks if the position is checkmate.
    pub fn is_checkmate(&self) -> bool {
        self.board.is_checkmate()
    }

    /// Checks if the position is stalemate.
    pub fn is_stalemate(&self) -> bool {
        self.board.is_stalemate()
    }

    /// Checks for insufficient material.
    pub fn is_insufficient_material(&self) -> bool {
        self.board.is_insufficient_material()
    }

    /// Checks for threefold repetition.
    pub fn is_threefold_repetition(&self) -> bool {
        if self.hash_history.len() < 5 {
            return false;
        }

        let Some(&current_hash) = self.hash_history.last() else {
            return false;
        };

        self.hash_history
            .iter()
            .filter(|&&h| h == current_hash)
            .count()
            >= 3
    }

    /// Checks for fifty-move rule.
    pub fn is_fifty_move_rule(&self) -> bool {
        self.board.halfmoves() >= 100
    }

    /// Parses a UCI move string.
    pub fn parse_uci_move(&self, uci: &str) -> Option<ChessMove> {
        let parsed: UciMove = uci.parse().ok()?;
        let chess_move = parsed.to_move(&self.board).ok()?;

        if !self.board.legal_moves().contains(&chess_move) {
            return None;
        }

        Some(ChessMove {
            inner: chess_move,
            variant: self.variant,
        })
    }

    /// Makes a move on the board.
    pub fn make_chess_move(&mut self, chess_move: &ChessMove) -> bool {
        if !self.board.legal_moves().contains(&chess_move.inner) {
            return false;
        }

        self.board.play_unchecked(&chess_move.inner);
        self.ply_count += 1;

        let hash: Zobrist64 = self.board.zobrist_hash(EnPassantMode::Legal);
        self.hash_history.push(hash.0);

        true
    }

    /// Makes a move from a UCI string.
    pub fn make_uci_move(&mut self, uci: &str) -> bool {
        if let Some(mv) = self.parse_uci_move(uci) {
            self.make_chess_move(&mv)
        } else {
            false
        }
    }

    /// Converts UCI to SAN.
    pub fn uci_to_san(&self, uci: &str) -> Option<String> {
        let parsed: UciMove = uci.parse().ok()?;
        let chess_move = parsed.to_move(&self.board).ok()?;
        let san = San::from_move(&self.board, &chess_move);
        Some(san.to_string())
    }

    /// Converts UCI to LAN.
    pub fn uci_to_lan(&self, uci: &str) -> Option<String> {
        let parsed: UciMove = uci.parse().ok()?;
        let chess_move = parsed.to_move(&self.board).ok()?;
        Some(move_to_lan(&chess_move))
    }
}

/// Converts a move to LAN notation.
fn move_to_lan(m: &Move) -> String {
    match m {
        Move::Normal {
            role,
            from,
            to,
            capture,
            promotion,
        } => {
            let mut lan = String::new();

            if *role != Role::Pawn {
                lan.push_str(&role.char().to_uppercase().to_string());
            }

            lan.push_str(&from.to_string());

            if capture.is_some() {
                lan.push('x');
            }

            lan.push_str(&to.to_string());

            if let Some(promo_role) = promotion {
                lan.push('=');
                lan.push_str(&promo_role.char().to_uppercase().to_string());
            }

            lan
        }
        Move::EnPassant { from, to, .. } => {
            let mut lan = String::new();
            lan.push_str(&from.to_string());
            lan.push('x');
            lan.push_str(&to.to_string());
            lan
        }
        Move::Castle { king, rook } => {
            if king.file() < rook.file() {
                "O-O".to_string()
            } else {
                "O-O-O".to_string()
            }
        }
        Move::Put { role, to } => {
            let mut lan = String::new();
            lan.push_str(&role.char().to_uppercase().to_string());
            lan.push('@');
            lan.push_str(&to.to_string());
            lan
        }
    }
}

// ── Game Trait Implementation ────────────────────────────────────────────────

impl Game for ChessGame {
    fn clone_box(&self) -> Box<dyn Game> {
        Box::new(self.clone())
    }

    fn variant(&self) -> VariantType {
        self.variant
    }

    fn side_to_move(&self) -> Color {
        match self.board.turn() {
            ChessColor::White => Color::First,
            ChessColor::Black => Color::Second,
        }
    }

    fn halfmove_clock(&self) -> u32 {
        self.board.halfmoves()
    }

    fn ply_count(&self) -> u32 {
        self.ply_count
    }

    fn status(&self) -> GameStatus {
        if let Some(outcome) = self.board.outcome() {
            return match outcome {
                shakmaty::Outcome::Decisive { winner } => GameStatus {
                    reason: GameOverReason::Checkmate,
                    winner: Some(match winner {
                        ChessColor::White => Color::First,
                        ChessColor::Black => Color::Second,
                    }),
                },
                shakmaty::Outcome::Draw => {
                    if self.board.is_stalemate() {
                        GameStatus {
                            reason: GameOverReason::Stalemate,
                            winner: None,
                        }
                    } else if self.board.is_insufficient_material() {
                        GameStatus {
                            reason: GameOverReason::InsufficientMaterial,
                            winner: None,
                        }
                    } else {
                        GameStatus {
                            reason: GameOverReason::Stalemate,
                            winner: None,
                        }
                    }
                }
            };
        }

        if self.is_insufficient_material() {
            return GameStatus {
                reason: GameOverReason::InsufficientMaterial,
                winner: None,
            };
        }

        if self.is_fifty_move_rule() {
            return GameStatus {
                reason: GameOverReason::FiftyMoveRule,
                winner: None,
            };
        }

        if self.is_threefold_repetition() {
            return GameStatus {
                reason: GameOverReason::Repetition,
                winner: None,
            };
        }

        GameStatus::ONGOING
    }

    fn parse_move(&self, notation: &str) -> Option<Box<dyn GameMove>> {
        self.parse_uci_move(notation)
            .map(|m| Box::new(m) as Box<dyn GameMove>)
    }

    fn make_move(&mut self, mv: &dyn GameMove) -> bool {
        // Downcast to ChessMove
        if let Some(chess_move) = mv.as_any().downcast_ref::<ChessMove>() {
            self.make_chess_move(chess_move)
        } else {
            false
        }
    }

    fn make_move_notation(&mut self, notation: &str) -> bool {
        if let Some(mv) = self.parse_uci_move(notation) {
            self.make_chess_move(&mv)
        } else {
            false
        }
    }

    fn fen(&self) -> String {
        Fen::from_position(self.board.clone(), EnPassantMode::Legal).to_string()
    }

    fn move_to_san(&self, notation: &str) -> Option<String> {
        self.uci_to_san(notation)
    }

    fn move_to_lan(&self, notation: &str) -> Option<String> {
        self.uci_to_lan(notation)
    }

    fn convert_move_to_san(&self, mv: &dyn GameMove) -> Option<String> {
        if let Some(chess_move) = mv.as_any().downcast_ref::<ChessMove>() {
            use shakmaty::san::San;
            let san = San::from_move(&self.board, &chess_move.inner());
            Some(san.to_string())
        } else {
            None
        }
    }

    fn convert_move_to_lan(&self, mv: &dyn GameMove) -> Option<String> {
        if let Some(chess_move) = mv.as_any().downcast_ref::<ChessMove>() {
            Some(move_to_lan(&chess_move.inner()))
        } else {
            None
        }
    }

    fn supports_syzygy(&self) -> bool {
        true
    }

    fn as_chess(&self) -> Option<&ChessGame> {
        Some(self)
    }

    fn as_shogi(&self) -> Option<&ShogiGame> {
        None
    }
}

// ── Tests ────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_game() {
        let game = ChessGame::new();
        assert_eq!(game.ply_count(), 0);
        assert_eq!(game.halfmove_clock(), 0);
        assert!(!game.is_check());
        assert!(!game.is_checkmate());
        assert!(!game.is_stalemate());
    }

    #[test]
    fn test_from_fen() {
        let fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        let game = ChessGame::from_fen(fen, VariantType::Standard).unwrap();
        assert_eq!(game.side_to_move(), Color::Second);
    }

    #[test]
    fn test_make_move() {
        let mut game = ChessGame::new();
        assert!(game.make_move_notation("e2e4"));
        assert_eq!(game.side_to_move(), Color::Second);
        assert_eq!(game.ply_count(), 1);
    }

    #[test]
    fn test_game_trait() {
        let mut game: Box<dyn Game> = Box::new(ChessGame::new());
        assert_eq!(game.variant(), VariantType::Standard);
        assert!(game.make_move_notation("e2e4"));
        assert!(game.status().is_ongoing());
    }
}
