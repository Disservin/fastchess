use serde::{Deserialize, Serialize};
use std::time::Duration;

use super::adjudication::*;
use super::enums::*;

/// SPRT (Sequential Probability Ratio Test) configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SprtConfig {
    pub enabled: bool,
    pub alpha: f64,
    pub beta: f64,
    pub elo0: f64,
    pub elo1: f64,
    /// Available models: "normalized", "bayesian", "logistic".
    pub model: String,
}

impl Default for SprtConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            alpha: 0.0,
            beta: 0.0,
            elo0: 0.0,
            elo1: 0.0,
            model: "normalized".to_string(),
        }
    }
}

/// Opening book configuration.
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct OpeningConfig {
    pub file: String,
    #[serde(default)]
    pub format: FormatType,
    #[serde(default)]
    pub order: OrderType,
    #[serde(default = "default_plies")]
    pub plies: i32,
    #[serde(default = "default_start")]
    pub start: usize,
}

fn default_plies() -> i32 {
    -1
}
fn default_start() -> usize {
    1
}

/// PGN output configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PgnConfig {
    pub additional_lines_rgx: Vec<String>,
    pub event_name: String,
    pub site: String,
    pub file: String,
    pub notation: NotationType,
    pub append_file: bool,
    pub track_nodes: bool,
    pub track_seldepth: bool,
    pub track_nps: bool,
    pub track_hashfull: bool,
    pub track_tbhits: bool,
    pub track_timeleft: bool,
    pub track_latency: bool,
    pub track_pv: bool,
    pub min: bool,
    pub crc: bool,
}

impl Default for PgnConfig {
    fn default() -> Self {
        Self {
            additional_lines_rgx: Vec::new(),
            event_name: "Fastchess Tournament".to_string(),
            site: "?".to_string(),
            file: String::new(),
            notation: NotationType::San,
            append_file: true,
            track_nodes: false,
            track_seldepth: false,
            track_nps: false,
            track_hashfull: false,
            track_tbhits: false,
            track_timeleft: false,
            track_latency: false,
            track_pv: false,
            min: false,
            crc: false,
        }
    }
}

/// EPD output configuration.
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct EpdConfig {
    #[serde(default)]
    pub file: String,
    #[serde(default = "default_true")]
    pub append_file: bool,
}

fn default_true() -> bool {
    true
}

/// Log level.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Default)]
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    #[default]
    Warn,
    Error,
    Fatal,
}

/// Logging configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LogConfig {
    pub file: String,
    pub level: LogLevel,
    pub append_file: bool,
    pub compress: bool,
    pub realtime: bool,
    pub engine_coms: bool,
}

impl Default for LogConfig {
    fn default() -> Self {
        Self {
            file: String::new(),
            level: LogLevel::Warn,
            append_file: true,
            compress: false,
            realtime: true,
            engine_coms: false,
        }
    }
}

/// Complete tournament configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TournamentConfig {
    pub opening: OpeningConfig,
    pub pgn: PgnConfig,
    pub epd: EpdConfig,
    pub sprt: SprtConfig,

    pub config_name: String,

    pub draw: DrawAdjudication,
    pub resign: ResignAdjudication,
    pub maxmoves: MaxMovesAdjudication,
    pub tb_adjudication: TbAdjudication,

    pub variant: VariantType,
    pub r#type: TournamentType,
    pub gauntlet_seeds: i32,

    pub output: OutputType,
    pub autosaveinterval: i32,
    pub ratinginterval: i32,
    pub games: usize,
    pub rounds: usize,
    pub report_penta: bool,

    pub seed: u64,
    pub scoreinterval: i32,
    pub concurrency: usize,
    pub wait: u32,
    pub force_concurrency: bool,

    pub noswap: bool,
    pub reverse: bool,
    pub recover: bool,
    pub affinity: bool,
    pub show_latency: bool,
    pub test_env: bool,

    /// Maximum time to wait for engine startup (UCI handshake).
    #[serde(skip)]
    pub startup_time: Duration,
    /// Maximum time to wait for ucinewgame acknowledgement.
    #[serde(skip)]
    pub ucinewgame_time: Duration,
    /// Maximum time to wait for isready/readyok ping.
    #[serde(skip)]
    pub ping_time: Duration,

    pub affinity_cpus: Vec<i32>,

    pub log: LogConfig,
}

impl Default for TournamentConfig {
    fn default() -> Self {
        Self {
            opening: OpeningConfig::default(),
            pgn: PgnConfig::default(),
            epd: EpdConfig::default(),
            sprt: SprtConfig::default(),
            config_name: "config.json".to_string(),
            draw: DrawAdjudication::default(),
            resign: ResignAdjudication::default(),
            maxmoves: MaxMovesAdjudication::default(),
            tb_adjudication: TbAdjudication::default(),
            variant: VariantType::Standard,
            r#type: TournamentType::RoundRobin,
            gauntlet_seeds: 1,
            output: OutputType::Fastchess,
            autosaveinterval: 20,
            ratinginterval: 10,
            games: 2,
            rounds: 2,
            report_penta: true,
            seed: rand::random(),
            scoreinterval: 1,
            concurrency: 1,
            wait: 0,
            force_concurrency: false,
            noswap: false,
            reverse: false,
            recover: false,
            affinity: false,
            show_latency: false,
            test_env: false,
            startup_time: Duration::from_secs(10),
            ucinewgame_time: Duration::from_secs(60),
            ping_time: Duration::from_secs(60),
            affinity_cpus: Vec::new(),
            log: LogConfig::default(),
        }
    }
}
