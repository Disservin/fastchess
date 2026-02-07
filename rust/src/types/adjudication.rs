use serde::{Deserialize, Serialize};

/// Draw adjudication settings.
///
/// When enabled, the game is declared a draw if both engines report scores
/// within `score` centipawns for `move_count` consecutive moves, starting
/// after move `move_number`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrawAdjudication {
    pub move_number: u32,
    pub move_count: i32,
    pub score: i32,
    pub enabled: bool,
}

impl Default for DrawAdjudication {
    fn default() -> Self {
        Self {
            move_number: 0,
            move_count: 1,
            score: 0,
            enabled: false,
        }
    }
}

/// Resign adjudication settings.
///
/// When enabled, the game is adjudicated as a loss for the resigning side
/// if the engine reports a score worse than `-score` centipawns for
/// `move_count` consecutive moves.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResignAdjudication {
    pub move_count: i32,
    pub score: i32,
    pub twosided: bool,
    pub enabled: bool,
}

impl Default for ResignAdjudication {
    fn default() -> Self {
        Self {
            move_count: 1,
            score: 0,
            twosided: false,
            enabled: false,
        }
    }
}

/// Max moves adjudication settings.
///
/// When enabled, the game is declared a draw after `move_count` moves.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaxMovesAdjudication {
    pub move_count: i32,
    pub enabled: bool,
}

impl Default for MaxMovesAdjudication {
    fn default() -> Self {
        Self {
            move_count: 1,
            enabled: false,
        }
    }
}

/// Syzygy tablebase adjudication result type.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TbResultType {
    WinLoss = 1,
    Draw = 2,
    Both = 3,
}

impl Default for TbResultType {
    fn default() -> Self {
        Self::Both
    }
}

/// Tablebase adjudication settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TbAdjudication {
    pub syzygy_dirs: String,
    pub max_pieces: i32,
    pub result_type: TbResultType,
    pub ignore_50_move_rule: bool,
    pub enabled: bool,
}

impl Default for TbAdjudication {
    fn default() -> Self {
        Self {
            syzygy_dirs: String::new(),
            max_pieces: 0,
            result_type: TbResultType::Both,
            ignore_50_move_rule: false,
            enabled: false,
        }
    }
}
