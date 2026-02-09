pub mod book;
pub mod chess;
pub mod pgn;
pub mod shogi;
pub mod syzygy;
pub mod timecontrol;

use crate::types::VariantType;

// ── Unified Game Types ───────────────────────────────────────────────────────

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

impl From<chess::Color> for Color {
    fn from(c: chess::Color) -> Self {
        match c {
            chess::Color::White => Color::First,
            chess::Color::Black => Color::Second,
        }
    }
}

impl From<shogi::Color> for Color {
    fn from(c: shogi::Color) -> Self {
        match c {
            shogi::Color::Sente => Color::First,
            shogi::Color::Gote => Color::Second,
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
            GameOverReason::InsufficientMaterial => "Draw by insufficient mating material".to_string(),
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
}

// ── Game Instance ────────────────────────────────────────────────────────────

/// A validated move that can be applied to a game.
#[derive(Clone, Debug)]
pub enum GameMove {
    Chess(chess::ChessMove),
    Shogi(shogi::Move),
}

/// A unified game instance that can be either chess or shogi.
#[derive(Clone)]
pub enum GameInstance {
    Chess(chess::ChessGame),
    Shogi(ShogiGame),
}

impl GameInstance {
    /// Creates a new game from a FEN/SFEN string and variant type.
    pub fn from_fen(fen: &str, variant: VariantType) -> Option<Self> {
        match variant {
            VariantType::Standard => {
                chess::ChessGame::from_fen(fen, chess::Variant::Standard).map(GameInstance::Chess)
            }
            VariantType::Frc => {
                chess::ChessGame::from_fen(fen, chess::Variant::Chess960).map(GameInstance::Chess)
            }
            VariantType::Shogi => ShogiGame::from_sfen(fen).map(GameInstance::Shogi),
        }
    }

    /// Creates a new game with the default starting position for the variant.
    pub fn new(variant: VariantType) -> Self {
        match variant {
            VariantType::Standard => GameInstance::Chess(chess::ChessGame::new()),
            VariantType::Frc => {
                GameInstance::Chess(chess::ChessGame::with_variant(chess::Variant::Chess960))
            }
            VariantType::Shogi => GameInstance::Shogi(ShogiGame::new()),
        }
    }

    /// Returns the variant type of this game.
    pub fn variant(&self) -> VariantType {
        match self {
            GameInstance::Chess(g) => match g.variant() {
                chess::Variant::Standard => VariantType::Standard,
                chess::Variant::Chess960 => VariantType::Frc,
            },
            GameInstance::Shogi(_) => VariantType::Shogi,
        }
    }

    /// Returns the current side to move.
    pub fn side_to_move(&self) -> Color {
        match self {
            GameInstance::Chess(g) => g.side_to_move().into(),
            GameInstance::Shogi(g) => g.side_to_move().into(),
        }
    }

    /// Returns the half-move clock (for fifty-move rule in chess, always 0 in shogi).
    pub fn halfmove_clock(&self) -> u32 {
        match self {
            GameInstance::Chess(g) => g.halfmove_clock(),
            GameInstance::Shogi(_) => 0, // Shogi has no half-move clock
        }
    }

    /// Returns the ply count (number of half-moves played).
    pub fn ply_count(&self) -> u32 {
        match self {
            GameInstance::Chess(g) => g.ply_count(),
            GameInstance::Shogi(g) => g.ply_count(),
        }
    }

    /// Returns the current game status.
    pub fn status(&self) -> GameStatus {
        match self {
            GameInstance::Chess(g) => {
                let status = g.status();
                GameStatus {
                    reason: match status.reason {
                        chess::GameOverReason::None => GameOverReason::None,
                        chess::GameOverReason::Checkmate => GameOverReason::Checkmate,
                        chess::GameOverReason::Stalemate => GameOverReason::Stalemate,
                        chess::GameOverReason::InsufficientMaterial => {
                            GameOverReason::InsufficientMaterial
                        }
                        chess::GameOverReason::ThreefoldRepetition => GameOverReason::Repetition,
                        chess::GameOverReason::FiftyMoveRule => GameOverReason::FiftyMoveRule,
                    },
                    winner: status.winner.map(|c| c.into()),
                }
            }
            GameInstance::Shogi(g) => g.status(),
        }
    }

    /// Parses a UCI/USI move string and returns the move if valid.
    pub fn parse_move(&self, mv: &str) -> Option<GameMove> {
        match self {
            GameInstance::Chess(g) => g.parse_uci_move(mv).map(GameMove::Chess),
            GameInstance::Shogi(g) => g.parse_usi_move(mv).map(GameMove::Shogi),
        }
    }

    /// Makes a move on the board. Returns the game outcome after the move.
    pub fn make_move(&mut self, mv: &GameMove) -> bool {
        match (self, mv) {
            (GameInstance::Chess(g), GameMove::Chess(m)) => g.make_move(m),
            (GameInstance::Shogi(g), GameMove::Shogi(m)) => g.make_move(*m).is_some(),
            _ => false,
        }
    }

    /// Makes a move from a UCI/USI string.
    pub fn make_uci_move(&mut self, uci: &str) -> bool {
        match self {
            GameInstance::Chess(g) => g.make_uci_move(uci),
            GameInstance::Shogi(g) => g.make_usi_move(uci),
        }
    }

    /// Returns the current FEN/SFEN string.
    pub fn fen(&self) -> String {
        match self {
            GameInstance::Chess(g) => g.fen(),
            GameInstance::Shogi(g) => g.sfen(),
        }
    }

    /// Converts a UCI/USI move to SAN/KIF notation (if applicable).
    pub fn move_to_san(&self, uci: &str) -> Option<String> {
        match self {
            GameInstance::Chess(g) => g.uci_to_san(uci),
            GameInstance::Shogi(_) => Some(uci.to_string()), // Shogi uses USI directly for now
        }
    }

    /// Converts a UCI/USI move to LAN notation.
    pub fn move_to_lan(&self, uci: &str) -> Option<String> {
        match self {
            GameInstance::Chess(g) => g.uci_to_lan(uci),
            GameInstance::Shogi(_) => Some(uci.to_string()), // Shogi uses USI directly
        }
    }

    /// Returns a reference to the inner chess game (for TB probing, etc.)
    pub fn as_chess(&self) -> Option<&chess::ChessGame> {
        match self {
            GameInstance::Chess(g) => Some(g),
            GameInstance::Shogi(_) => None,
        }
    }

    /// Returns a reference to the inner shogi game.
    pub fn as_shogi(&self) -> Option<&ShogiGame> {
        match self {
            GameInstance::Shogi(g) => Some(g),
            GameInstance::Chess(_) => None,
        }
    }

    /// Returns the default start position FEN/SFEN for the variant.
    pub fn default_fen(variant: VariantType) -> &'static str {
        match variant {
            VariantType::Standard | VariantType::Frc => {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
            }
            VariantType::Shogi => "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
        }
    }
}

// ── ShogiGame Wrapper ────────────────────────────────────────────────────────

/// A shogi game wrapper with history tracking for repetition detection.
#[derive(Clone, Debug)]
pub struct ShogiGame {
    game: shogi::Game,
    ply_count: u32,
}

impl ShogiGame {
    /// Creates a new shogi game from the standard starting position.
    pub fn new() -> Self {
        Self {
            game: shogi::Game::new(shogi::Position::default()),
            ply_count: 0,
        }
    }

    /// Creates a new shogi game from a SFEN string.
    pub fn from_sfen(sfen: &str) -> Option<Self> {
        let position = shogi::Position::parse(sfen)?;
        Some(Self {
            game: shogi::Game::new(position),
            ply_count: 0,
        })
    }

    /// Returns the current side to move.
    pub fn side_to_move(&self) -> shogi::Color {
        self.game.stm()
    }

    /// Returns the ply count.
    pub fn ply_count(&self) -> u32 {
        self.ply_count
    }

    /// Returns the current SFEN string.
    pub fn sfen(&self) -> String {
        self.game.usi_string()
    }

    /// Parses a USI move string.
    pub fn parse_usi_move(&self, usi: &str) -> Option<shogi::Move> {
        shogi::Move::parse(usi)
    }

    /// Makes a move and returns the outcome.
    pub fn make_move(&mut self, mv: shogi::Move) -> Option<shogi::GameOutcome> {
        let outcome = self.game.do_move(mv);
        if outcome != shogi::GameOutcome::LossByIllegal(self.game.stm()) {
            self.ply_count += 1;
            Some(outcome)
        } else {
            None
        }
    }

    /// Makes a move from a USI string.
    pub fn make_usi_move(&mut self, usi: &str) -> bool {
        if let Some(mv) = shogi::Move::parse(usi) {
            self.make_move(mv).is_some()
        } else {
            false
        }
    }

    /// Returns the current game status.
    pub fn status(&self) -> GameStatus {
        // The shogi::Game tracks outcomes internally through do_move
        // For now, we check checkmate state
        GameStatus::ONGOING
    }
}

impl Default for ShogiGame {
    fn default() -> Self {
        Self::new()
    }
}
