//! Chess variant implementation.
//!
//! Implements the `Game` trait for standard chess and Chess960 using the chess_library_rs library.

use crate::game::{GameOverReason, GameStatus, Side};
use crate::types::VariantType;
use crate::variants::{Game, GameMove};
use chess_library_rs::{
    self,
    board::Board,
    board::GameResult,
    board::GameResultReason,
    chess_move::Move,
    color::Color as ChessColor,
    uci::{move_to_lan, uci_to_move},
};

// Re-export types that other modules need
pub use crate::variants::shogi::ShogiGame;

/// A validated chess move.
#[derive(Clone)]
pub struct ChessMove {
    inner: Move,
    variant: VariantType,
}

impl ChessMove {
    /// Returns the UCI representation of this move.
    pub fn to_uci(&self) -> String {
        let chess960 = matches!(self.variant, VariantType::Frc);
        chess_library_rs::uci::move_to_uci(self.inner, chess960)
    }

    /// Returns the internal chess_library_rs move.
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
            let san = chess_library_rs::uci::move_to_san(&chess_game.board, self.inner);
            Some(san)
        } else {
            None
        }
    }

    fn to_lan(&self, game: &dyn crate::variants::Game) -> Option<String> {
        Some(move_to_lan(&game.as_chess()?.board, self.inner))
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
    board: Board,
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
        let board = Board::new();
        let initial_hash = board.hash();

        Self {
            board,
            variant: VariantType::Standard,
            hash_history: vec![initial_hash],
            ply_count: 0,
        }
    }

    /// Creates a new game from a FEN string.
    pub fn from_fen(fen: &str, variant: VariantType) -> Option<Self> {
        let mut board = Board::from_fen(fen);

        // Set chess960 mode for FRC variant
        if variant == VariantType::Frc {
            board.set_960(true);
        }

        // Validate that the FEN was parsed correctly by checking if board is valid
        // (Board::from_fen always returns a board, but we should verify it's usable)
        let initial_hash = board.hash();

        Some(Self {
            board,
            variant,
            hash_history: vec![initial_hash],
            ply_count: 0,
        })
    }

    /// Creates a new game with the given variant.
    pub fn with_variant(variant: VariantType) -> Self {
        let mut board = Board::new();

        // Set chess960 mode for FRC variant
        if variant == VariantType::Frc {
            board.set_960(true);
        }

        let initial_hash = board.hash();

        Self {
            board,
            variant,
            hash_history: vec![initial_hash],
            ply_count: 0,
        }
    }

    /// Returns the variant.
    pub fn variant_type(&self) -> VariantType {
        self.variant
    }

    /// Returns a reference to the underlying chess_library_rs board.
    pub fn inner(&self) -> &Board {
        &self.board
    }

    /// Checks if the position is in check.
    pub fn is_check(&self) -> bool {
        self.board.in_check()
    }

    /// Checks if the position is checkmate.
    pub fn is_checkmate(&self) -> bool {
        let (reason, _) = self.board.is_game_over();
        reason == GameResultReason::Checkmate
    }

    /// Checks if the position is stalemate.
    pub fn is_stalemate(&self) -> bool {
        let (reason, _) = self.board.is_game_over();
        reason == GameResultReason::Stalemate
    }

    /// Checks for insufficient material.
    pub fn is_insufficient_material(&self) -> bool {
        self.board.is_insufficient_material()
    }

    /// Checks for threefold repetition.
    pub fn is_threefold_repetition(&self) -> bool {
        self.board.is_repetition(2)
    }

    /// Checks for fifty-move rule.
    pub fn is_fifty_move_rule(&self) -> bool {
        self.board.is_half_move_draw()
    }

    /// Parses a UCI move string.
    pub fn parse_uci_move(&self, uci: &str) -> Option<ChessMove> {
        let chess_move = chess_library_rs::uci::uci_to_move(&self.board, uci);

        // Check if move is valid (not NO_MOVE)
        if chess_move.raw() == Move::NO_MOVE {
            return None;
        }

        // Verify the move is legal by checking if it's in the legal moves list
        let mut movelist = chess_library_rs::movelist::Movelist::new();
        chess_library_rs::movegen::legalmoves(
            &mut movelist,
            &self.board,
            chess_library_rs::movegen::MoveGenType::All,
            chess_library_rs::movegen::PieceGenType::ALL,
        );

        if !movelist.iter().any(|m| m.raw() == chess_move.raw()) {
            return None;
        }

        Some(ChessMove {
            inner: chess_move,
            variant: self.variant,
        })
    }

    /// Makes a move on the board.
    pub fn make_chess_move(&mut self, chess_move: &ChessMove) -> bool {
        // Verify the move is legal
        let mut movelist = chess_library_rs::movelist::Movelist::new();
        chess_library_rs::movegen::legalmoves(
            &mut movelist,
            &self.board,
            chess_library_rs::movegen::MoveGenType::All,
            chess_library_rs::movegen::PieceGenType::ALL,
        );

        if !movelist.iter().any(|m| m.raw() == chess_move.inner.raw()) {
            return false;
        }

        self.board.make_move(chess_move.inner, false);
        self.ply_count += 1;

        let hash = self.board.hash();
        self.hash_history.push(hash);

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
        let chess_move = chess_library_rs::uci::uci_to_move(&self.board, uci);
        if chess_move.raw() == Move::NO_MOVE {
            return None;
        }
        let san = chess_library_rs::uci::move_to_san(&self.board, chess_move);
        Some(san)
    }

    /// Converts UCI to LAN.
    pub fn uci_to_lan(&self, uci: &str) -> Option<String> {
        Some(move_to_lan(&self.board, uci_to_move(&self.board, uci)))
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

    fn side_to_move(&self) -> Side {
        match self.board.side_to_move() {
            ChessColor::White => Side::White,
            ChessColor::Black => Side::Black,
            ChessColor::None => Side::White, // Default to White if None
        }
    }

    fn halfmove_clock(&self) -> u32 {
        self.board.half_move_clock()
    }

    fn ply_count(&self) -> u32 {
        self.ply_count
    }

    fn status(&self) -> GameStatus {
        let (reason, result) = self.board.is_game_over();

        // TODO CHECK
        match result {
            GameResult::Win => {
                assert!(false, "Chess should never report a win for the side to move - it should report a loss for the side to move instead");
                return GameStatus {
                    reason: GameOverReason::None,
                };
            }
            GameResult::Lose => GameStatus {
                reason: GameOverReason::Checkmate,
            },
            GameResult::Draw => {
                let draw_reason = match reason {
                    GameResultReason::Stalemate => GameOverReason::Stalemate,
                    GameResultReason::InsufficientMaterial => GameOverReason::InsufficientMaterial,
                    GameResultReason::FiftyMoveRule => GameOverReason::FiftyMoveRule,
                    GameResultReason::ThreefoldRepetition => GameOverReason::Repetition,
                    _ => GameOverReason::Stalemate,
                };

                GameStatus {
                    reason: draw_reason,
                }
            }
            GameResult::None => {
                // Check other draw conditions that might not be caught by is_game_over
                if self.is_insufficient_material() {
                    return GameStatus {
                        reason: GameOverReason::InsufficientMaterial,
                    };
                }

                if self.is_fifty_move_rule() {
                    return GameStatus {
                        reason: GameOverReason::FiftyMoveRule,
                    };
                }

                if self.is_threefold_repetition() {
                    return GameStatus {
                        reason: GameOverReason::Repetition,
                    };
                }

                GameStatus::ONGOING
            }
        }
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
        self.board.get_fen(true)
    }

    fn move_to_san(&self, notation: &str) -> Option<String> {
        self.uci_to_san(notation)
    }

    fn move_to_lan(&self, notation: &str) -> Option<String> {
        self.uci_to_lan(notation)
    }

    fn convert_move_to_san(&self, mv: &dyn GameMove) -> Option<String> {
        if let Some(chess_move) = mv.as_any().downcast_ref::<ChessMove>() {
            let san = chess_library_rs::uci::move_to_san(&self.board, chess_move.inner);
            Some(san)
        } else {
            None
        }
    }

    fn convert_move_to_lan(&self, mv: &dyn GameMove) -> Option<String> {
        if let Some(chess_move) = mv.as_any().downcast_ref::<ChessMove>() {
            Some(move_to_lan(&self.board, chess_move.inner))
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

    fn is_threefold_repetition(&self) -> bool {
        self.is_threefold_repetition()
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
        assert_eq!(game.side_to_move(), Side::Black);
    }

    #[test]
    fn test_make_move() {
        let mut game = ChessGame::new();
        assert!(game.make_move_notation("e2e4"));
        assert_eq!(game.side_to_move(), Side::Black);
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
