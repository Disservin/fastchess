//! UCI/USI engine management — the protocol layer between process I/O and game logic.
//!
//! Ports `engine/uci_engine.hpp` and `engine/uci_engine.cpp` from C++.
//!
//! This module manages a chess/shogi engine that speaks the UCI or USI protocol.
//! It wraps a [`Process`] and provides high-level methods such as [`UciEngine::start`],
//! [`UciEngine::go`], [`UciEngine::ucinewgame`], etc.

use std::fmt::Write;
use std::time::Duration;

use crate::core::logger::LOGGER;
use crate::core::str_utils;
use crate::engine::option::{parse_uci_option_line, OptionType, UCIOptions};
use crate::engine::process::{Line, Process, ProcessError, ProcessResult, Standard};
use crate::engine::protocol::Protocol;
use crate::game::timecontrol::TimeControl;
use crate::types::engine_config::EngineConfiguration;
use crate::types::enums::VariantType;
use crate::{log_trace, log_warn};

// ── Score types ──────────────────────────────────────────────────────────────

/// The kind of score reported by the engine in `info` lines.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ScoreType {
    Cp,
    Mate,
    Err,
}

/// A score extracted from the engine's `info` output.
#[derive(Debug, Clone, Copy)]
pub struct Score {
    pub score_type: ScoreType,
    pub value: i64,
}

/// Result of parsing the `bestmove` line from an engine.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BestMoveResult {
    /// A normal move (UCI move string like "e2e4", "7g7f", etc.)
    Move(String),
    /// USI: Engine declares a win (e.g., checkmate delivered)
    Win,
    /// USI: Engine resigns
    Resign,
}

/// All data extracted from an engine's `info` line for PGN metadata.
#[derive(Debug, Clone, Default)]
pub struct InfoData {
    pub depth: i64,
    pub seldepth: i64,
    pub nodes: u64,
    pub nps: u64,
    pub hashfull: i64,
    pub tbhits: u64,
    pub pv: String,
}

// ── Side to move (lightweight, avoids pulling in the full chess crate) ───────

/// Side to move, used to map `wtime`/`btime` correctly.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Color {
    White,
    Black,
}

// ── Default timing constants ─────────────────────────────────────────────────
// These mirror the C++ `config::TournamentConfig` defaults.  When a proper
// config system is ported they should come from there instead.

const DEFAULT_PING_TIME: Duration = Duration::from_millis(10_000);
const DEFAULT_UCINEWGAME_TIME: Duration = Duration::from_millis(30_000);
const DEFAULT_STARTUP_TIME: Duration = Duration::from_millis(10_000);

// ── UciEngine ────────────────────────────────────────────────────────────────

/// Manages a single chess/shogi engine process via the UCI or USI protocol.
pub struct UciEngine {
    process: Process,
    uci_options: UCIOptions,
    config: EngineConfiguration,
    output: Vec<Line>,
    initialized: bool,
    realtime_logging: bool,
    protocol: Protocol,
    cpus: Option<Vec<i32>>,
}

impl UciEngine {
    /// Create a new `UciEngine` from an engine configuration.
    ///
    /// Does **not** start the engine process — call [`start`] for that.
    pub fn new(config: &EngineConfiguration, realtime_logging: bool) -> Result<Self, ProcessError> {
        let mut process = Process::new()?;
        process.set_realtime_logging(realtime_logging);
        let protocol = Protocol::new(config.variant);

        Ok(Self {
            process,
            uci_options: UCIOptions::new(),
            config: config.clone(),
            output: Vec::with_capacity(100),
            initialized: false,
            realtime_logging,
            protocol,
            cpus: None,
        })
    }

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /// Start the engine, initialise the UCI handshake, and optionally pin it to
    /// specific CPUs.  Does nothing after the first call.
    ///
    /// Returns `Ok(true)` on success, `Err(message)` on failure.
    pub fn start(&mut self, cpus: Option<&[i32]>) -> Result<bool, String> {
        if self.initialized {
            return Ok(true);
        }

        self.cpus = cpus.map(|c| c.to_vec());

        log_trace!(
            "Starting engine {} at {}",
            self.config.name,
            self.config.cmd
        );

        let res = self.process.init(
            &self.config.dir,
            &self.config.cmd,
            &self.config.args,
            &self.config.name,
        );
        if let Err(e) = res {
            return Err(e.to_string());
        }

        if let Some(cpus) = cpus {
            self.set_cpus(cpus);
        }

        self.initialized = true;

        // UCI handshake
        if !self.uci() {
            return Err("Couldn't write uci to engine".to_string());
        }

        if !self.uciok(Some(Self::get_startup_time())) {
            return Err("Engine didn't respond to uciok after startup".to_string());
        }

        Ok(true)
    }

    /// Restart/refresh the engine for a new game: send `ucinewgame`, reapply
    /// options, and optionally set `UCI_Chess960` for FRC.
    pub fn refresh_uci(&mut self) -> bool {
        log_trace!("Refreshing engine {}", self.config.name);

        if !self.ucinewgame() {
            log_warn!(
                "Engine {} failed to start/refresh ({}).",
                self.config.name,
                self.protocol.newgame_cmd()
            );
            return false;
        }

        // Send Threads first to help multi-threaded engines
        let mut options = self.config.options.clone();
        options.sort_by(|a, b| {
            let a_is_threads = a.0 == "Threads";
            let b_is_threads = b.0 == "Threads";
            b_is_threads.cmp(&a_is_threads)
        });

        for (name, value) in &options {
            // Use protocol wrapper to translate option names (e.g., Hash -> USI_Hash)
            let option_name = self.protocol.translate_option_name(name);
            self.send_setoption(&option_name, value);
        }

        if self.config.variant == VariantType::Frc {
            self.send_setoption("UCI_Chess960", "true");
        }

        log_trace!("Engine {} refreshed.", self.config.name);
        true
    }

    /// Send `stop` and `quit` to the engine.
    pub fn quit(&mut self) {
        if !self.initialized {
            return;
        }
        let _ = self.write_engine("stop");
        let _ = self.write_engine("quit");
    }

    /// Restart the engine process completely.
    ///
    /// Kills the current process, creates a new one, and reinitializes UCI.
    /// Returns `Ok(true)` on success, `Err(message)` on failure.
    pub fn restart(&mut self) -> Result<bool, String> {
        log_trace!("Restarting engine {}", self.config.name);

        self.quit();

        // Create a new process (old one is dropped/killed)
        let mut new_process = Process::new().map_err(|e| e.to_string())?;
        new_process.set_realtime_logging(self.realtime_logging);
        self.process = new_process;

        // Reset state
        self.initialized = false;
        self.uci_options = UCIOptions::new();
        self.output.clear();

        // Re-initialize
        self.start(self.cpus.clone().as_deref())
    }

    // ── UCI commands ─────────────────────────────────────────────────────────

    /// Send the `uci` or `usi` initialization command.
    pub fn uci(&mut self) -> bool {
        log_trace!(
            "Sending {} to engine {}",
            self.protocol.init_cmd(),
            self.config.name
        );
        self.write_engine(self.protocol.init_cmd())
    }

    /// Wait for `uciok` or `usiok`, parsing `option` lines along the way.
    pub fn uciok(&mut self, threshold: Option<Duration>) -> bool {
        log_trace!(
            "Waiting for {} from engine {}",
            self.protocol.init_ok(),
            self.config.name
        );

        let res = self.read_engine(self.protocol.init_ok(), threshold);
        let ok = res.is_ok();

        // Log output if using deferred logging
        if !self.realtime_logging {
            for line in &self.output {
                LOGGER.read_from_engine(
                    &line.line,
                    &line.time,
                    &self.config.name,
                    line.std == Standard::Err,
                    None,
                );
            }
        }

        // Parse UCI option lines from stdout
        let stdout_lines: Vec<String> = self
            .get_stdout_lines()
            .iter()
            .map(|l| l.line.clone())
            .collect();

        for line_str in &stdout_lines {
            if let Some(option) = parse_uci_option_line(line_str) {
                self.uci_options.add_option(option);
            }
        }

        if !ok {
            log_warn!(
                "Engine {} did not respond to uciok in time.",
                self.config.name
            );
        }

        ok
    }

    /// Send newgame command
    pub fn ucinewgame(&mut self) -> bool {
        log_trace!(
            "Sending {} to engine {}",
            self.protocol.newgame_cmd(),
            self.config.name
        );

        if !self.write_engine(self.protocol.newgame_cmd()) {
            log_warn!(
                "Failed to send {} to engine {}",
                self.protocol.newgame_cmd(),
                self.config.name
            );
            return false;
        }

        self.isready(Some(Self::get_ucinewgame_time())).is_ok()
    }

    /// Send `isready` and wait for `readyok`.
    pub fn isready(&mut self, threshold: Option<Duration>) -> ProcessResult {
        if let Err(e) = self.process.alive() {
            return Err(format!("Engine process is not alive: {}", e).into());
        }

        let cmd = self.protocol.isready_cmd();

        if cmd.is_empty() {
            return Ok(());
        }

        log_trace!("Pinging engine {} with {}", self.config.name, cmd);

        if !self.write_engine(cmd) {
            return Err("Failed to write isready".into());
        }

        self.process.setup_read();

        let mut output = Vec::new();

        let res = self.process.read_output(
            &mut output,
            self.protocol.isready_ok(),
            threshold.unwrap_or(Self::get_ping_time()),
        );

        // Log deferred output
        if !self.realtime_logging {
            for line in &output {
                LOGGER.read_from_engine(
                    &line.line,
                    &line.time,
                    &self.config.name,
                    line.std == Standard::Err,
                    None,
                );
            }
        }

        if res.is_err() {
            if !crate::is_stop() {
                log_trace!("Engine {} didn't respond to isready", self.config.name);
                log_warn!("Warning; Engine {} is not responsive", self.config.name);
            }
            return res;
        }

        log_trace!("Engine {} is responsive", self.config.name);
        res
    }

    /// Send a `position` command with a FEN/SFEN and optional move list.
    pub fn position(&mut self, moves: &[String], fen: &str) -> bool {
        let pos_cmd = self.protocol.position_cmd(fen, moves);
        self.write_engine(&pos_cmd)
    }

    /// Send a `go` command with time controls and engine limits.
    ///
    /// The protocol wrapper handles the mapping of time parameters correctly
    /// for UCI (White moves first) and USI (Black/Sente moves first).
    pub fn go(&mut self, our_tc: &TimeControl, enemy_tc: &TimeControl, stm: Color) -> bool {
        let mut input = String::from("go");

        if self.config.limit.nodes > 0 {
            let _ = write!(input, " nodes {}", self.config.limit.nodes);
        }

        if self.config.limit.plies > 0 {
            let _ = write!(input, " depth {}", self.config.limit.plies);
        }

        // Fixed time per move (movetime) — cannot combine with tc
        if our_tc.is_fixed_time() {
            let _ = write!(input, " movetime {}", our_tc.get_fixed_time());
            return self.write_engine(&input);
        }

        // Map side-to-move to first/second player time controls
        let (first_tc, second_tc) = match stm {
            Color::White => (our_tc, enemy_tc),
            Color::Black => (enemy_tc, our_tc),
        };

        // Use protocol wrapper to get correct time parameter names
        if our_tc.is_timed() || our_tc.is_increment() {
            if first_tc.is_timed() || first_tc.is_increment() {
                let _ = write!(
                    input,
                    " {} {}",
                    self.protocol.first_player_time(),
                    first_tc.get_time_left()
                );
            }
            if second_tc.is_timed() || second_tc.is_increment() {
                let _ = write!(
                    input,
                    " {} {}",
                    self.protocol.second_player_time(),
                    second_tc.get_time_left()
                );
            }
        }

        if our_tc.is_increment() {
            if first_tc.is_increment() {
                let _ = write!(
                    input,
                    " {} {}",
                    self.protocol.first_player_inc(),
                    first_tc.get_increment()
                );
            }
            if second_tc.is_increment() {
                let _ = write!(
                    input,
                    " {} {}",
                    self.protocol.second_player_inc(),
                    second_tc.get_increment()
                );
            }
        }

        if our_tc.is_moves() {
            let _ = write!(input, " movestogo {}", our_tc.get_moves_left());
        }

        self.write_engine(&input)
    }

    // ── I/O helpers ──────────────────────────────────────────────────────────

    /// Write a UCI command to the engine (appends `\n`).
    pub fn write_engine(&mut self, input: &str) -> bool {
        LOGGER.write_to_engine(input, "", &self.config.name, None);
        self.process.write_input(&format!("{}\n", input)).is_ok()
    }

    /// Read engine output until `last_word` appears at the start of a line.
    pub fn read_engine(&mut self, last_word: &str, threshold: Option<Duration>) -> ProcessResult {
        self.process.setup_read();
        self.process.read_output(
            &mut self.output,
            last_word,
            threshold.unwrap_or(Self::get_ping_time()),
        )
    }

    /// Flush deferred engine output to the logger.
    pub fn write_log(&self) {
        for line in &self.output {
            LOGGER.read_from_engine(
                &line.line,
                &line.time,
                &self.config.name,
                line.std == Standard::Err,
                None,
            );
        }
    }

    /// Set CPU affinity for the engine process.
    pub fn set_cpus(&self, cpus: &[i32]) {
        if cpus.is_empty() {
            return;
        }

        // macOS does not support setting affinity per-PID
        #[cfg(target_os = "macos")]
        return;

        #[cfg(not(target_os = "macos"))]
        {
            if self.process.set_affinity(cpus) {
                return;
            }

            let cpu_str: String = cpus
                .iter()
                .map(|c| c.to_string())
                .collect::<Vec<_>>()
                .join(", ");

            log_warn!(
                "Warning; Failed to set CPU affinity for the engine process to {}. Please restart.",
                cpu_str
            );
        }
    }

    // ── Output queries ───────────────────────────────────────────────────────

    /// Get the `id name` reported by the engine.
    pub fn id_name(&mut self) -> Option<String> {
        if !self.uci() {
            log_warn!(
                "Warning; Engine {} didn't respond to uci.",
                self.config.name
            );
            return None;
        }
        if !self.uciok(None) {
            log_warn!(
                "Warning; Engine {} didn't respond to uci.",
                self.config.name
            );
            return None;
        }

        for line in self.get_stdout_lines() {
            if let Some(pos) = line.line.find("id name") {
                return Some(line.line[pos + 8..].to_string());
            }
        }
        None
    }

    /// Get the `id author` reported by the engine.
    pub fn id_author(&mut self) -> Option<String> {
        if !self.uci() {
            log_warn!(
                "Warning; Engine {} didn't respond to uci.",
                self.config.name
            );
            return None;
        }
        if !self.uciok(None) {
            log_warn!(
                "Warning; Engine {} didn't respond to uci.",
                self.config.name
            );
            return None;
        }

        for line in self.get_stdout_lines() {
            if let Some(pos) = line.line.find("id author") {
                return Some(line.line[pos + 10..].to_string());
            }
        }
        None
    }

    /// Extract the `bestmove` from the last output.
    ///
    /// Returns `None` if no bestmove line is found.
    /// For USI engines, handles special cases `bestmove win` and `bestmove resign`.
    pub fn bestmove(&self) -> Option<BestMoveResult> {
        let stdout = self.get_stdout_lines();
        if stdout.is_empty() {
            log_warn!("Warning; No output from {}", self.config.name);
            return None;
        }

        let last_line = &stdout.last()?.line;
        let tokens = str_utils::split_string(last_line, ' ');
        let move_str: String = str_utils::find_element(&tokens, "bestmove")?;

        // Handle USI special cases using protocol wrapper
        if self.protocol.is_bestmove_win(&move_str) {
            Some(BestMoveResult::Win)
        } else if self.protocol.is_bestmove_resign(&move_str) {
            Some(BestMoveResult::Resign)
        } else {
            Some(BestMoveResult::Move(move_str))
        }
    }

    /// Get the last `info` line (with score, preferring non-bound lines).
    pub fn last_info_line(&self) -> String {
        let mut fallback = String::new();

        let lines = self.get_stdout_lines();
        for line in lines.iter().rev() {
            let s = &line.line;

            // Skip "info string" lines
            if s.contains("info string") {
                continue;
            }

            // Only consider info lines with score (and multipv 1 if present)
            if !s.contains("info") || !s.contains(" score ") {
                continue;
            }
            if s.contains(" multipv ") && !s.contains(" multipv 1") {
                continue;
            }

            let is_bound = s.contains("lowerbound") || s.contains("upperbound");

            if !is_bound {
                return s.clone();
            }

            if fallback.is_empty() {
                fallback = s.clone();
            }
        }

        fallback
    }

    /// Parse the last info line into tokens.
    pub fn last_info(&self) -> Option<Vec<String>> {
        let info = self.last_info_line();
        if info.is_empty() {
            return None;
        }
        Some(str_utils::split_string(&info, ' '))
    }

    /// Get the last reported `time` value from info lines.
    pub fn last_time(&self) -> Duration {
        let lines = self.get_stdout_lines();
        for line in lines.iter().rev() {
            let s = &line.line;
            if s.contains("info string") {
                continue;
            }
            if !s.contains("info") || !s.contains("time") {
                continue;
            }
            let tokens = str_utils::split_string(s, ' ');
            let time_ms: i64 = str_utils::find_element(&tokens, "time").unwrap_or(0);
            return Duration::from_millis(time_ms.max(0) as u64);
        }
        Duration::ZERO
    }

    /// Extract the score from the last info output.
    pub fn last_score(&self) -> Result<Score, String> {
        let info = self.last_info().ok_or_else(|| {
            format!(
                "No info line available to extract score from: {}",
                self.last_info_line()
            )
        })?;

        let type_str: String = str_utils::find_element_result(&info, "score")?;

        let score_type = match type_str.as_str() {
            "cp" => ScoreType::Cp,
            "mate" => ScoreType::Mate,
            _ => return Err(format!("Unexpected score type: {}", self.last_info_line())),
        };

        let keyword = match score_type {
            ScoreType::Cp => "cp",
            ScoreType::Mate => "mate",
            ScoreType::Err => unreachable!(),
        };

        let value: i64 = str_utils::find_element_result(&info, keyword)?;

        Ok(Score { score_type, value })
    }

    /// Extract all info data (depth, seldepth, nodes, nps, hashfull, tbhits, pv) from the last info line.
    ///
    /// This is used to populate PGN move comments when tracking is enabled.
    pub fn last_info_data(&self) -> InfoData {
        let Some(info) = self.last_info() else {
            return InfoData::default();
        };

        // Extract PV: everything after "pv" until end of line or next known keyword
        let pv = Self::extract_pv(&info);

        InfoData {
            depth: str_utils::find_element(&info, "depth").unwrap_or(0),
            seldepth: str_utils::find_element(&info, "seldepth").unwrap_or(0),
            nodes: str_utils::find_element(&info, "nodes").unwrap_or(0),
            nps: str_utils::find_element(&info, "nps").unwrap_or(0),
            hashfull: str_utils::find_element(&info, "hashfull").unwrap_or(0),
            tbhits: str_utils::find_element(&info, "tbhits").unwrap_or(0),
            pv,
        }
    }

    /// Extract the PV (principal variation) from info tokens.
    ///
    /// The PV is a space-separated list of moves that appears after "pv" in the info line.
    fn extract_pv(tokens: &[String]) -> String {
        let Some(pv_pos) = tokens.iter().position(|s| s == "pv") else {
            return String::new();
        };

        // Collect all tokens after "pv" - these are the PV moves
        // PV continues to end of line (bestmove is on a separate line)
        tokens[pv_pos + 1..].join(" ")
    }

    /// Check if the engine output includes a `bestmove` line.
    pub fn output_includes_bestmove(&self) -> bool {
        self.get_stdout_lines()
            .iter()
            .any(|l| l.line.contains("bestmove"))
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    /// The full captured output from the last read cycle.
    pub fn output(&self) -> &[Line] {
        &self.output
    }

    /// The engine configuration.
    pub fn config(&self) -> &EngineConfiguration {
        &self.config
    }

    /// The parsed UCI options reported by the engine.
    pub fn uci_options(&self) -> &UCIOptions {
        &self.uci_options
    }

    pub fn uci_options_mut(&mut self) -> &mut UCIOptions {
        &mut self.uci_options
    }

    pub fn is_realtime_logging(&self) -> bool {
        self.realtime_logging
    }

    /// The protocol wrapper for this engine (UCI or USI).
    pub fn protocol(&self) -> &Protocol {
        &self.protocol
    }

    // ── Timing defaults ──────────────────────────────────────────────────────

    pub fn get_ping_time() -> Duration {
        DEFAULT_PING_TIME
    }

    pub fn get_ucinewgame_time() -> Duration {
        DEFAULT_UCINEWGAME_TIME
    }

    pub fn get_startup_time() -> Duration {
        DEFAULT_STARTUP_TIME
    }

    // ── Private helpers ──────────────────────────────────────────────────────

    fn send_setoption(&mut self, name: &str, value: &str) {
        let Some(option) = self.uci_options.get_option(name) else {
            log_warn!("Warning; {} doesn't have option {}", self.config.name, name);
            return;
        };

        if !option.is_valid(value) {
            log_warn!("Warning; Invalid value for option {}; {}", name, value);
            return;
        }

        let opt_type = option.option_type();

        log_trace!(
            "Sending setoption to engine {} {} {}",
            self.config.name,
            name,
            value
        );

        if opt_type == OptionType::Button {
            if value != "true" {
                return;
            }
            if !self.write_engine(&format!("setoption name {}", name)) {
                log_warn!(
                    "Failed to send setoption to engine {} {}",
                    self.config.name,
                    name
                );
                return;
            }
            if let Some(opt) = self.uci_options.get_option_mut(name) {
                opt.set_value(value);
            }
            return;
        }

        if !self.write_engine(&format!("setoption name {} value {}", name, value)) {
            log_warn!(
                "Failed to send setoption to engine {} {} {}",
                self.config.name,
                name,
                value
            );
            return;
        }

        if let Some(opt) = self.uci_options.get_option_mut(name) {
            opt.set_value(value);
        }
    }

    /// Filter output lines to only stdout.
    fn get_stdout_lines(&self) -> Vec<&Line> {
        self.output
            .iter()
            .filter(|l| l.std == Standard::Output)
            .collect()
    }

    /// Filter output lines to only stderr.
    #[allow(dead_code)]
    fn get_stderr_lines(&self) -> Vec<&Line> {
        self.output
            .iter()
            .filter(|l| l.std == Standard::Err)
            .collect()
    }
}

#[cfg(test)]
impl UciEngine {
    /// Inject a fake stdout line for testing (e.g., info lines with scores).
    pub fn inject_output_line(&mut self, line: &str) {
        self.output.push(Line {
            line: line.to_string(),
            time: String::new(),
            std: Standard::Output,
        });
    }
}

impl Drop for UciEngine {
    fn drop(&mut self) {
        self.quit();
    }
}
