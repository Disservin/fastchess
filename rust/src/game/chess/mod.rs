//! Chess game abstraction module.
//!
//! This module provides a high-level abstraction over the chess rules, hiding
//! the underlying shakmaty implementation details. It includes built-in support
//! for threefold repetition detection and other draw conditions.

use shakmaty::fen::Fen;
use shakmaty::san::San;
use shakmaty::uci::UciMove;
use shakmaty::zobrist::{Zobrist64, ZobristHash};
use shakmaty::{CastlingMode, Chess, Color as ChessColor, EnPassantMode, Move, Position, Role};

// ── Game Status Types ────────────────────────────────────────────────────────

/// The side to move in a chess game.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Color {
    White,
    Black,
}

impl Color {
    /// Returns the opposite color.
    pub fn opposite(self) -> Self {
        match self {
            Color::White => Color::Black,
            Color::Black => Color::White,
        }
    }

    /// Returns a human-readable name for the color.
    pub fn name(self) -> &'static str {
        match self {
            Color::White => "White",
            Color::Black => "Black",
        }
    }
}

impl From<ChessColor> for Color {
    fn from(c: ChessColor) -> Self {
        match c {
            ChessColor::White => Color::White,
            ChessColor::Black => Color::Black,
        }
    }
}

impl From<Color> for ChessColor {
    fn from(c: Color) -> Self {
        match c {
            Color::White => ChessColor::White,
            Color::Black => ChessColor::Black,
        }
    }
}

/// Reason the game ended from the chess rules perspective.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GameOverReason {
    /// Game is not over.
    None,
    /// Checkmate.
    Checkmate,
    /// Stalemate (no legal moves, not in check).
    Stalemate,
    /// Insufficient material to mate.
    InsufficientMaterial,
    /// Threefold repetition.
    ThreefoldRepetition,
    /// Fifty-move rule (100 half-moves without pawn move or capture).
    FiftyMoveRule,
}

impl GameOverReason {
    /// Returns a human-readable message for this game-over reason.
    ///
    /// For checkmate, pass the color of the winner.
    pub fn message(self, winner: Option<Color>) -> &'static str {
        match self {
            GameOverReason::None => "",
            GameOverReason::Checkmate => match winner {
                Some(Color::White) => "White mates",
                Some(Color::Black) => "Black mates",
                None => "Checkmate",
            },
            GameOverReason::Stalemate => "Draw by stalemate",
            GameOverReason::InsufficientMaterial => "Draw by insufficient mating material",
            GameOverReason::ThreefoldRepetition => "Draw by 3-fold repetition",
            GameOverReason::FiftyMoveRule => "Draw by fifty moves rule",
        }
    }

    /// Returns true if this reason represents a draw.
    pub fn is_draw(self) -> bool {
        matches!(
            self,
            GameOverReason::Stalemate
                | GameOverReason::InsufficientMaterial
                | GameOverReason::ThreefoldRepetition
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
}

// ── Chess Game ───────────────────────────────────────────────────────────────

/// Variant type for chess games.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Variant {
    #[default]
    Standard,
    Chess960,
}

impl Variant {
    fn to_castling_mode(self) -> CastlingMode {
        match self {
            Variant::Standard => CastlingMode::Standard,
            Variant::Chess960 => CastlingMode::Chess960,
        }
    }
}

/// A chess game with position tracking and automatic threefold repetition detection.
///
/// This struct wraps the shakmaty `Chess` position and maintains a history of
/// Zobrist hashes for detecting threefold repetition.
#[derive(Clone)]
pub struct ChessGame {
    /// The current board position.
    board: Chess,
    /// The variant (Standard or Chess960).
    variant: Variant,
    /// Zobrist hash history for threefold repetition detection.
    hash_history: Vec<u64>,
    /// Ply counter (half-moves played).
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
            variant: Variant::Standard,
            hash_history: vec![initial_hash.0],
            ply_count: 0,
        }
    }

    /// Creates a new game from a FEN string.
    ///
    /// Returns `None` if the FEN is invalid.
    pub fn from_fen(fen: &str, variant: Variant) -> Option<Self> {
        let parsed: Fen = fen.parse().ok()?;
        let board: Chess = parsed.into_position(variant.to_castling_mode()).ok()?;
        let initial_hash: Zobrist64 = board.zobrist_hash(EnPassantMode::Legal);

        Some(Self {
            board,
            variant,
            hash_history: vec![initial_hash.0],
            ply_count: 0,
        })
    }

    /// Creates a new game from the standard starting position with the given variant.
    pub fn with_variant(variant: Variant) -> Self {
        let board = Chess::default();
        let initial_hash: Zobrist64 = board.zobrist_hash(EnPassantMode::Legal);

        Self {
            board,
            variant,
            hash_history: vec![initial_hash.0],
            ply_count: 0,
        }
    }

    /// Returns the current side to move.
    pub fn side_to_move(&self) -> Color {
        self.board.turn().into()
    }

    /// Returns the half-move clock (for fifty-move rule).
    pub fn halfmove_clock(&self) -> u32 {
        self.board.halfmoves()
    }

    /// Returns the ply count (number of half-moves played).
    pub fn ply_count(&self) -> u32 {
        self.ply_count
    }

    /// Returns the variant of this game.
    pub fn variant(&self) -> Variant {
        self.variant
    }

    /// Returns the current FEN string.
    pub fn fen(&self) -> String {
        shakmaty::fen::Fen::from_position(self.board.clone(), EnPassantMode::Legal).to_string()
    }

    /// Returns a reference to the underlying shakmaty Chess position.
    ///
    /// This is provided for advanced use cases like Syzygy tablebase probing.
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

    /// Checks if there is insufficient material to mate.
    pub fn is_insufficient_material(&self) -> bool {
        self.board.is_insufficient_material()
    }

    /// Checks for threefold repetition.
    ///
    /// Returns true if the current position has occurred at least 3 times.
    pub fn is_threefold_repetition(&self) -> bool {
        // Need at least 5 positions for threefold repetition to be possible
        // (position 1, moves, position 2, moves, position 3)
        if self.hash_history.len() < 5 {
            return false;
        }

        let Some(&current_hash) = self.hash_history.last() else {
            return false;
        };

        let count = self
            .hash_history
            .iter()
            .filter(|&&h| h == current_hash)
            .count();
        count >= 3
    }

    /// Checks if the fifty-move rule applies.
    ///
    /// Returns true if 100 half-moves have been made without a pawn move or capture.
    pub fn is_fifty_move_rule(&self) -> bool {
        self.board.halfmoves() >= 100
    }

    /// Returns the current game status.
    ///
    /// This checks all standard chess rules including checkmate, stalemate,
    /// insufficient material, fifty-move rule, and threefold repetition.
    pub fn status(&self) -> GameStatus {
        // Check for checkmate / stalemate via outcome
        if let Some(outcome) = self.board.outcome() {
            return match outcome {
                shakmaty::Outcome::Decisive { winner } => GameStatus {
                    reason: GameOverReason::Checkmate,
                    winner: Some(winner.into()),
                },
                shakmaty::Outcome::Draw => {
                    // Determine draw reason
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

        // Check insufficient material (shakmaty's outcome() may not catch all cases)
        if self.board.is_insufficient_material() {
            return GameStatus {
                reason: GameOverReason::InsufficientMaterial,
                winner: None,
            };
        }

        // Check fifty-move rule (halfmoves >= 100 half-moves = 50 full moves)
        if self.is_fifty_move_rule() {
            return GameStatus {
                reason: GameOverReason::FiftyMoveRule,
                winner: None,
            };
        }

        // Check threefold repetition
        if self.is_threefold_repetition() {
            return GameStatus {
                reason: GameOverReason::ThreefoldRepetition,
                winner: None,
            };
        }

        GameStatus::ONGOING
    }

    /// Parses a UCI move string and returns the internal move if valid.
    ///
    /// Returns `None` if the move is invalid or illegal.
    pub fn parse_uci_move(&self, uci: &str) -> Option<ChessMove> {
        let parsed: UciMove = uci.parse().ok()?;
        let chess_move = parsed.to_move(&self.board).ok()?;

        // Check legality
        if !self.board.legal_moves().contains(&chess_move) {
            return None;
        }

        Some(ChessMove(chess_move))
    }

    /// Makes a move on the board.
    ///
    /// Returns `true` if the move was legal and applied, `false` otherwise.
    pub fn make_move(&mut self, chess_move: &ChessMove) -> bool {
        // Verify the move is legal
        if !self.board.legal_moves().contains(&chess_move.0) {
            return false;
        }

        self.board.play_unchecked(&chess_move.0);
        self.ply_count += 1;

        // Record position hash for repetition detection
        let hash: Zobrist64 = self.board.zobrist_hash(EnPassantMode::Legal);
        self.hash_history.push(hash.0);

        true
    }

    /// Makes a move from a UCI string.
    ///
    /// Returns `true` if the move was legal and applied, `false` otherwise.
    pub fn make_uci_move(&mut self, uci: &str) -> bool {
        let Some(chess_move) = self.parse_uci_move(uci) else {
            return false;
        };
        self.make_move(&chess_move)
    }

    /// Returns the number of legal moves in the current position.
    pub fn legal_move_count(&self) -> usize {
        self.board.legal_moves().len()
    }

    /// Converts a UCI move to SAN notation.
    ///
    /// Returns `None` if the move is invalid.
    pub fn uci_to_san(&self, uci: &str) -> Option<String> {
        let parsed: UciMove = uci.parse().ok()?;
        let chess_move = parsed.to_move(&self.board).ok()?;
        let san = San::from_move(&self.board, &chess_move);
        Some(san.to_string())
    }

    /// Converts a UCI move to LAN (Long Algebraic Notation).
    ///
    /// Returns `None` if the move is invalid.
    pub fn uci_to_lan(&self, uci: &str) -> Option<String> {
        let parsed: UciMove = uci.parse().ok()?;
        let chess_move = parsed.to_move(&self.board).ok()?;
        Some(Self::move_to_lan(&chess_move))
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

                // Piece symbol (uppercase), except for pawns
                if *role != Role::Pawn {
                    lan.push_str(&role.char().to_uppercase().to_string());
                }

                // Origin square (always included in LAN)
                lan.push_str(&from.to_string());

                // Capture symbol
                if capture.is_some() {
                    lan.push('x');
                }

                // Destination square
                lan.push_str(&to.to_string());

                // Promotion
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

    /// Returns the hash history for external use (e.g., debugging).
    pub fn hash_history(&self) -> &[u64] {
        &self.hash_history
    }
}

// ── Chess Move ───────────────────────────────────────────────────────────────

/// A validated chess move.
///
/// This type wraps the internal shakmaty Move type and ensures
/// the move has been validated as legal.
#[derive(Clone)]
pub struct ChessMove(Move);

impl ChessMove {
    /// Returns the UCI representation of this move.
    pub fn to_uci(&self, variant: Variant) -> String {
        UciMove::from_move(&self.0, variant.to_castling_mode()).to_string()
    }
}

impl std::fmt::Debug for ChessMove {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "ChessMove({:?})", self.0)
    }
}

// ── Tests ────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_game() {
        let game = ChessGame::new();
        assert_eq!(game.side_to_move(), Color::White);
        assert_eq!(game.ply_count(), 0);
        assert_eq!(game.halfmove_clock(), 0);
        assert!(!game.is_check());
        assert!(!game.is_checkmate());
        assert!(!game.is_stalemate());
    }

    #[test]
    fn test_from_fen() {
        let fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        let game = ChessGame::from_fen(fen, Variant::Standard).unwrap();
        assert_eq!(game.side_to_move(), Color::Black);
    }

    #[test]
    fn test_make_move() {
        let mut game = ChessGame::new();
        assert!(game.make_uci_move("e2e4"));
        assert_eq!(game.side_to_move(), Color::Black);
        assert_eq!(game.ply_count(), 1);
    }

    #[test]
    fn test_illegal_move() {
        let mut game = ChessGame::new();
        assert!(!game.make_uci_move("e1e5")); // King can't move there
        assert_eq!(game.ply_count(), 0);
    }

    #[test]
    fn test_checkmate_detection() {
        // Fool's mate position - white is checkmated
        let fen = "rnb1kbnr/pppp1ppp/4p3/8/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3";
        let game = ChessGame::from_fen(fen, Variant::Standard).unwrap();
        assert!(game.is_checkmate());
        let status = game.status();
        assert_eq!(status.reason, GameOverReason::Checkmate);
        assert_eq!(status.winner, Some(Color::Black));
    }

    #[test]
    fn test_stalemate_detection() {
        // Classic stalemate: black king on a8, white king on c7, white queen on b6
        // Black has no legal moves but is not in check
        let fen = "k7/2K5/1Q6/8/8/8/8/8 b - - 0 1";
        let game = ChessGame::from_fen(fen, Variant::Standard).unwrap();
        assert!(game.is_stalemate());
        let status = game.status();
        assert_eq!(status.reason, GameOverReason::Stalemate);
        assert_eq!(status.winner, None); // Stalemate is a draw
    }

    #[test]
    fn test_threefold_repetition() {
        let mut game = ChessGame::new();

        // Knights bouncing back and forth
        let moves = [
            "g1f3", "g8f6", "f3g1", "f6g8", // First repetition of start position
            "g1f3", "g8f6", "f3g1", "f6g8", // Second repetition = threefold
        ];

        for mv in &moves {
            assert!(game.make_uci_move(mv), "Failed to make move: {}", mv);
        }

        assert!(game.is_threefold_repetition());
        let status = game.status();
        assert_eq!(status.reason, GameOverReason::ThreefoldRepetition);
    }

    #[test]
    fn test_no_threefold_repetition_early() {
        let mut game = ChessGame::new();

        // Only one round of knight moves
        let moves = ["g1f3", "g8f6", "f3g1", "f6g8"];

        for mv in &moves {
            assert!(game.make_uci_move(mv));
        }

        // Only 2 occurrences of the position, not 3
        assert!(!game.is_threefold_repetition());
    }

    #[test]
    fn test_uci_to_san() {
        let game = ChessGame::new();
        assert_eq!(game.uci_to_san("e2e4"), Some("e4".to_string()));
        assert_eq!(game.uci_to_san("g1f3"), Some("Nf3".to_string()));
    }

    #[test]
    fn test_uci_to_lan() {
        let game = ChessGame::new();
        assert_eq!(game.uci_to_lan("e2e4"), Some("e2e4".to_string()));
        assert_eq!(game.uci_to_lan("g1f3"), Some("Ng1f3".to_string()));
    }

    #[test]
    fn test_color_opposite() {
        assert_eq!(Color::White.opposite(), Color::Black);
        assert_eq!(Color::Black.opposite(), Color::White);
    }

    #[test]
    fn test_game_over_reason_messages() {
        assert_eq!(
            GameOverReason::Checkmate.message(Some(Color::White)),
            "White mates"
        );
        assert_eq!(GameOverReason::Stalemate.message(None), "Draw by stalemate");
        assert_eq!(
            GameOverReason::ThreefoldRepetition.message(None),
            "Draw by 3-fold repetition"
        );
    }

    #[test]
    fn test_insufficient_material() {
        // King vs King
        let fen = "8/8/8/4k3/8/8/4K3/8 w - - 0 1";
        let game = ChessGame::from_fen(fen, Variant::Standard).unwrap();
        assert!(game.is_insufficient_material());
        let status = game.status();
        assert_eq!(status.reason, GameOverReason::InsufficientMaterial);
    }

    #[test]
    fn test_variant_chess960() {
        let game = ChessGame::with_variant(Variant::Chess960);
        assert_eq!(game.variant(), Variant::Chess960);
    }
}
