use super::engine_config::*;
use super::enums::*;

/// Data captured for a single move in a game.
#[derive(Debug, Clone, Default)]
pub struct MoveData {
    pub additional_lines: Vec<String>,
    pub r#move: String,
    pub score_string: String,
    pub pv: String,

    pub nodes: u64,
    pub nps: u64,
    pub tbhits: u64,

    pub elapsed_millis: i64,
    pub depth: i64,
    pub seldepth: i64,
    pub score: i64,
    pub timeleft: i64,
    pub latency: i64,
    pub hashfull: i64,

    pub legal: bool,
    pub book: bool,
}

/// Reason a match terminated.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MatchTermination {
    Normal,
    Adjudication,
    Timeout,
    Disconnect,
    Stall,
    IllegalMove,
    Interrupt,
    None,
}

impl Default for MatchTermination {
    fn default() -> Self {
        Self::None
    }
}

/// Game result from a player's perspective.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum GameResult {
    Win,
    Lose,
    Draw,
    #[default]
    None,
}

/// Information about a player in a finished match.
#[derive(Debug, Clone)]
pub struct PlayerInfo {
    pub config: EngineConfiguration,
    pub result: GameResult,
}

/// All data from a completed match.
#[derive(Debug, Clone)]
pub struct MatchData {
    pub players: GamePair<PlayerInfo>,

    pub start_time: String,
    pub end_time: String,
    pub duration: String,
    pub date: String,
    pub fen: String,

    /// Human-readable reason the game ended (printed to console and PGN comment).
    pub reason: String,

    pub moves: Vec<MoveData>,

    /// Machine-readable termination category (used for PGN header).
    pub termination: MatchTermination,

    pub variant: VariantType,

    pub needs_restart: bool,
}

impl Default for MatchData {
    fn default() -> Self {
        Self {
            players: GamePair::new(
                PlayerInfo {
                    config: EngineConfiguration::default(),
                    result: GameResult::None,
                },
                PlayerInfo {
                    config: EngineConfiguration::default(),
                    result: GameResult::None,
                },
            ),
            start_time: String::new(),
            end_time: String::new(),
            duration: String::new(),
            date: String::new(),
            fen: String::new(),
            reason: String::new(),
            moves: Vec::new(),
            termination: MatchTermination::None,
            variant: VariantType::Standard,
            needs_restart: false,
        }
    }
}

impl MatchData {
    pub fn new(fen: &str) -> Self {
        let now = chrono::Local::now();
        Self {
            fen: fen.to_string(),
            start_time: now.to_rfc3339(),
            date: now.format("%Y.%m.%d").to_string(),
            ..Default::default()
        }
    }

    pub fn get_losing_player(&self) -> Option<&PlayerInfo> {
        if self.players.white.result == GameResult::Lose {
            Some(&self.players.white)
        } else if self.players.black.result == GameResult::Lose {
            Some(&self.players.black)
        } else {
            Option::None
        }
    }

    pub fn get_winning_player(&self) -> Option<&PlayerInfo> {
        if self.players.white.result == GameResult::Win {
            Some(&self.players.white)
        } else if self.players.black.result == GameResult::Win {
            Some(&self.players.black)
        } else {
            Option::None
        }
    }
}
