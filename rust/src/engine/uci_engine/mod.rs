//! UCI engine management — the protocol layer between process I/O and game logic.
//!
//! Ports `engine/uci_engine.hpp` and `engine/uci_engine.cpp` from C++.
//!
//! This module manages a chess engine that speaks the UCI protocol.  It wraps a
//! [`Process`] and provides high-level methods such as [`UciEngine::start`],
//! [`UciEngine::go`], [`UciEngine::ucinewgame`], etc.

use std::time::Duration;

use crate::core::logger::LOGGER;
use crate::core::str_utils;
use crate::engine::option::{parse_uci_option_line, OptionType, UCIOptions};
use crate::engine::process::{Line, Process, ProcessResult, Standard, Status};
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

/// Manages a single chess engine process via the UCI protocol.
pub struct UciEngine {
    process: Process,
    uci_options: UCIOptions,
    config: EngineConfiguration,
    output: Vec<Line>,
    initialized: bool,
    realtime_logging: bool,
}

impl UciEngine {
    /// Create a new `UciEngine` from an engine configuration.
    ///
    /// Does **not** start the engine process — call [`start`] for that.
    pub fn new(config: &EngineConfiguration, realtime_logging: bool) -> Self {
        let mut process = Process::new();
        process.set_realtime_logging(realtime_logging);

        Self {
            process,
            uci_options: UCIOptions::new(),
            config: config.clone(),
            output: Vec::with_capacity(100),
            initialized: false,
            realtime_logging,
        }
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
        if !res.is_ok() {
            return Err(res.message);
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
                "Engine {} failed to start/refresh (ucinewgame).",
                self.config.name
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
            self.send_setoption(name, value);
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

    // ── UCI commands ─────────────────────────────────────────────────────────

    /// Send the `uci` command.
    pub fn uci(&mut self) -> bool {
        log_trace!("Sending uci to engine {}", self.config.name);
        self.write_engine("uci")
    }

    /// Wait for `uciok`, parsing `option` lines along the way.
    pub fn uciok(&mut self, threshold: Option<Duration>) -> bool {
        log_trace!("Waiting for uciok from engine {}", self.config.name);

        let res = self.read_engine("uciok", threshold);
        let ok = res.code == Status::Ok;

        // Log output if using deferred logging
        if !self.realtime_logging {
            for line in &self.output {
                LOGGER.read_from_engine(
                    &line.line,
                    &line.time,
                    &self.config.name,
                    line.std == Standard::Err,
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

    /// Send `ucinewgame` and wait for `readyok`.
    pub fn ucinewgame(&mut self) -> bool {
        log_trace!("Sending ucinewgame to engine {}", self.config.name);

        if !self.write_engine("ucinewgame") {
            log_warn!("Failed to send ucinewgame to engine {}", self.config.name);
            return false;
        }

        self.isready(Some(Self::get_ucinewgame_time())).code == Status::Ok
    }

    /// Send `isready` and wait for `readyok`.
    pub fn isready(&mut self, threshold: Option<Duration>) -> ProcessResult {
        let is_alive = self.process.alive();
        if !is_alive.is_ok() {
            return is_alive;
        }

        log_trace!("Pinging engine {}", self.config.name);

        if !self.write_engine("isready") {
            return ProcessResult::error("Failed to write isready");
        }

        self.process.setup_read();

        let mut output = Vec::new();
        let res = self.process.read_output(
            &mut output,
            "readyok",
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
                );
            }
        }

        if !res.is_ok() {
            if !crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
                log_trace!("Engine {} didn't respond to isready", self.config.name);
                log_warn!("Warning; Engine {} is not responsive", self.config.name);
            }
            return res;
        }

        log_trace!("Engine {} is responsive", self.config.name);
        res
    }

    /// Send a `position` command with a FEN and optional move list.
    pub fn position(&mut self, moves: &[String], fen: &str) -> bool {
        let mut pos_cmd = if fen == "startpos" {
            "position startpos".to_string()
        } else {
            format!("position fen {}", fen)
        };

        if !moves.is_empty() {
            pos_cmd.push_str(" moves");
            for mv in moves {
                pos_cmd.push(' ');
                pos_cmd.push_str(mv);
            }
        }

        self.write_engine(&pos_cmd)
    }

    /// Send a `go` command with time controls and engine limits.
    pub fn go(&mut self, our_tc: &TimeControl, enemy_tc: &TimeControl, stm: Color) -> bool {
        let mut input = String::from("go");

        if self.config.limit.nodes > 0 {
            input.push_str(&format!(" nodes {}", self.config.limit.nodes));
        }

        if self.config.limit.plies > 0 {
            input.push_str(&format!(" depth {}", self.config.limit.plies));
        }

        // Fixed time per move (movetime) — cannot combine with tc
        if our_tc.is_fixed_time() {
            input.push_str(&format!(" movetime {}", our_tc.get_fixed_time()));
            return self.write_engine(&input);
        }

        let (white, black) = match stm {
            Color::White => (our_tc, enemy_tc),
            Color::Black => (enemy_tc, our_tc),
        };

        if our_tc.is_timed() || our_tc.is_increment() {
            if white.is_timed() || white.is_increment() {
                input.push_str(&format!(" wtime {}", white.get_time_left()));
            }
            if black.is_timed() || black.is_increment() {
                input.push_str(&format!(" btime {}", black.get_time_left()));
            }
        }

        if our_tc.is_increment() {
            if white.is_increment() {
                input.push_str(&format!(" winc {}", white.get_increment()));
            }
            if black.is_increment() {
                input.push_str(&format!(" binc {}", black.get_increment()));
            }
        }

        if our_tc.is_moves() {
            input.push_str(&format!(" movestogo {}", our_tc.get_moves_left()));
        }

        self.write_engine(&input)
    }

    // ── I/O helpers ──────────────────────────────────────────────────────────

    /// Write a UCI command to the engine (appends `\n`).
    pub fn write_engine(&mut self, input: &str) -> bool {
        LOGGER.write_to_engine(input, "", &self.config.name);
        self.process.write_input(&format!("{}\n", input)).code == Status::Ok
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
    pub fn bestmove(&self) -> Option<String> {
        let stdout = self.get_stdout_lines();
        if stdout.is_empty() {
            log_warn!("Warning; No output from {}", self.config.name);
            return None;
        }

        let last_line = &stdout.last()?.line;
        let tokens = str_utils::split_string(last_line, ' ');
        str_utils::find_element::<String>(&tokens, "bestmove")
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

impl Drop for UciEngine {
    fn drop(&mut self) {
        self.quit();
    }
}
