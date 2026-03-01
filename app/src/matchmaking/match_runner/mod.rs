//! Match runner — orchestrates a single game between two UCI engines.
//!
//! Ports `matchmaking/match/match.hpp` and `match.cpp` from C++.
//!
//! The `Match` struct handles the full lifecycle:
//! 1. Set up the board from the opening position
//! 2. Start both engines
//! 3. Run the game loop (alternating moves)
//! 4. Handle adjudication, timeouts, disconnects, illegal moves
//! 5. Produce `MatchData` with the result

pub mod trackers;

use std::time::{Duration, Instant};

use crate::engine::process::ProcessResultExt;
use crate::engine::uci_engine::{BestMoveResult, Score, ScoreType, UciEngine};
use crate::game::book::Opening;
use crate::game::{GameInstance, GameOverReason, GameStatus, Side};
use crate::matchmaking::player::Player;
use crate::types::engine_config::GamePair;
use crate::types::match_data::*;
use crate::types::VariantType;
use crate::{log_fatal, log_warn};

use trackers::*;

// ── Constants ────────────────────────────────────────────────────────────────

const ILLEGAL_MSG: &str = " makes an illegal move";
const ADJUDICATION_WIN_MSG: &str = " wins by adjudication";
const ADJUDICATION_TB_WIN_MSG: &str = " wins by adjudication: SyzygyTB";
const ADJUDICATION_TB_DRAW_MSG: &str = "Draw by adjudication: SyzygyTB";
const ADJUDICATION_MSG: &str = "Draw by adjudication";
const TIMEOUT_MSG: &str = " loses on time";
const DISCONNECT_MSG: &str = " disconnects";
const STALL_MSG: &str = "'s connection stalls";
const INTERRUPTED_MSG: &str = "Game interrupted";
const ENGINE_WIN_MSG: &str = " wins by engine declaration";
const ENGINE_RESIGN_MSG: &str = " resigns";

/// Simple game-over check for PV verification.
/// Returns (reason, is_game_over).
fn is_game_over_simple(game: &GameInstance) -> (GameOverReason, bool) {
    // Check for checkmate/stalemate by looking at game status
    let status = game.status();

    // we ignore insufficient material here since it's rather uncommon
    if status.is_game_over() && status.reason != GameOverReason::InsufficientMaterial {
        return (status.reason, true);
    }

    (GameOverReason::None, false)
}

// ── Match ────────────────────────────────────────────────────────────────────

/// Orchestrates a single game between two UCI engines.
pub struct Match {
    data: MatchData,
    draw_tracker: DrawTracker,
    resign_tracker: ResignTracker,
    maxmoves_tracker: MaxMovesTracker,
    tb_tracker: TbAdjudicationTracker,
    uci_moves: Vec<String>,
    start_position: String,
    stall_or_disconnect: bool,
    /// The game state with built-in repetition detection.
    game: GameInstance,
    /// The variant type (Standard, Chess960, or Shogi).
    variant: VariantType,
    /// Which color is to move at the start (after applying opening).
    /// Used to determine which player plays white.
    side_to_move_at_start: Side,
    /// Whether we're in test environment (affects log formatting).
    test_env: bool,
    /// Whether to check mate PVs for correct length and checkmate ending.
    check_mate_pvs: bool,
}

impl Match {
    /// Create a new match from the given opening.
    ///
    /// Reads adjudication settings from the global tournament config.
    pub fn new(
        opening: Opening,
        tournament_config: &crate::types::tournament::TournamentConfig,
    ) -> Self {
        let variant = tournament_config.variant;

        let fen_str = if let Some(fen) = opening.fen_epd.as_ref() {
            fen.clone()
        } else {
            GameInstance::default_fen(variant).to_string()
        };

        let start_position = fen_str.clone();

        // Create the game from FEN
        let mut game =
            GameInstance::from_fen(&fen_str, variant).unwrap_or_else(|| GameInstance::new(variant));

        let data = MatchData::new(&fen_str);

        // Apply opening book moves and collect UCI strings
        let mut uci_moves = Vec::new();
        let mut match_data = data;

        for mv_uci in &opening.moves {
            // Parse UCI move and apply it to the game
            let m = game.parse_move(mv_uci);
            if game.make_uci_move(mv_uci) {
                let move_data = MoveData {
                    r#move: m,
                    score_string: "0.00".to_string(),
                    book: true,
                    legal: true,
                    ..Default::default()
                };
                match_data.moves.push(move_data);
                uci_moves.push(mv_uci.clone());
            }
        }

        // Capture which color is to move after applying the opening
        let side_to_move_at_start = game.side_to_move();

        Self {
            data: match_data,
            draw_tracker: DrawTracker::new(&tournament_config.draw),
            resign_tracker: ResignTracker::new(&tournament_config.resign),
            maxmoves_tracker: MaxMovesTracker::new(&tournament_config.maxmoves),
            tb_tracker: TbAdjudicationTracker::new(&tournament_config.tb_adjudication),
            uci_moves,
            start_position,
            stall_or_disconnect: false,
            game,
            variant,
            side_to_move_at_start,
            test_env: tournament_config.test_env,
            check_mate_pvs: tournament_config.check_mate_pvs,
        }
    }

    /// Current side to move (from the game).
    fn side_to_move(&self) -> Side {
        self.game.side_to_move()
    }

    /// Side to move at the start of the game (after applying opening).
    /// Used to determine which player plays white.
    fn side_to_move_at_start(&self) -> Side {
        self.side_to_move_at_start
    }

    /// Half-move clock (for fifty-move rule) from the game.
    fn half_move_clock(&self) -> u32 {
        self.game.halfmove_clock()
    }

    /// Ply count from the game.
    fn ply_count(&self) -> u32 {
        self.game.ply_count()
    }

    fn full_move_number(&self) -> u32 {
        1 + self.ply_count() / 2
    }

    /// Start the match between two engines.
    ///
    /// This is the main entry point. It starts the engines, runs the game loop,
    /// and populates the `MatchData` with the result.
    ///
    /// The `first` and `second` parameters refer to the player order from the pairing,
    /// not colors. The actual color assignment (who plays white/black) is determined
    /// by the opening position after applying opening moves.
    pub fn start(&mut self, first: &mut UciEngine, second: &mut UciEngine, cpus: Option<&[i32]>) {
        // Create players in pairing order
        let mut first_player = Player::new(first);
        let mut second_player = Player::new(second);

        // Start engines
        if let Err(e) = first_player.engine.start(cpus) {
            if crate::is_stop() {
                return;
            }

            crate::set_stop();
            crate::set_abnormal_termination();

            log_fatal!(
                "Fatal; {} engine startup failure: \"{}\"",
                first_player.engine.config().name,
                e
            );
            return;
        }

        if let Err(e) = second_player.engine.start(cpus) {
            if crate::is_stop() {
                return;
            }

            crate::set_stop();
            crate::set_abnormal_termination();

            log_fatal!(
                "Fatal; {} engine startup failure: \"{}\"",
                second_player.engine.config().name,
                e
            );
            return;
        }

        if crate::is_stop() {
            return;
        }

        // Refresh UCI
        if !first_player.engine.refresh_uci() {
            self.set_engine_crash_status(&mut first_player, &mut second_player);
        }

        if !second_player.engine.refresh_uci() {
            self.set_engine_crash_status(&mut second_player, &mut first_player);
        }

        // Check connection
        self.valid_connection(&mut first_player, &mut second_player);

        // Determine which player played white based on the opening
        let first_played_white = self.side_to_move_at_start() == Side::White;

        if first_played_white {
            first_player.set_first_side();
            second_player.set_second_side();
        } else {
            first_player.set_second_side();
            second_player.set_first_side();
        }

        let start_time = Instant::now();

        if self.data.termination == MatchTermination::None {
            // Run the game loop with first/second players
            // The loop determines color assignment from the opening position
            self.game_loop(&mut first_player, &mut second_player);
        }

        let elapsed = start_time.elapsed();

        self.data.variant = self.variant;
        self.data.end_time = crate::core::datetime_iso();
        self.data.duration = crate::core::duration_string(elapsed);

        // Map results to white/black based on who actually played which color
        let (white_info, black_info) = if first_played_white {
            (
                PlayerInfo {
                    config: first_player.engine.config().clone(),
                    result: first_player.result(),
                    first_move: Some(true),
                },
                PlayerInfo {
                    config: second_player.engine.config().clone(),
                    result: second_player.result(),
                    first_move: Some(false),
                },
            )
        } else {
            (
                PlayerInfo {
                    config: second_player.engine.config().clone(),
                    result: second_player.result(),
                    first_move: Some(false),
                },
                PlayerInfo {
                    config: first_player.engine.config().clone(),
                    result: first_player.result(),
                    first_move: Some(true),
                },
            )
        };

        self.data.players = GamePair::new(white_info, black_info);
    }

    /// The core game loop.
    ///
    /// Runs the game with first/second players. Determines turn order from the
    /// opening position - whichever color is to move first determines which
    /// player moves first.
    fn game_loop<'a>(&mut self, first: &mut Player<'a>, second: &mut Player<'a>) {
        loop {
            if crate::is_stop() {
                self.data.termination = MatchTermination::Interrupt;
                break;
            }

            if !self.play_move(first, second) {
                break;
            }

            if crate::is_stop() {
                self.data.termination = MatchTermination::Interrupt;
                break;
            }

            if !self.play_move(second, first) {
                break;
            }
        }

        debug_assert!(self.data.termination != MatchTermination::None);
    }

    /// Play a single move. Returns `false` if the game should end.
    fn play_move(&mut self, us: &mut Player, them: &mut Player) -> bool {
        // Check game over from board state
        let status = self.game.status();

        if status.is_game_over() {
            if status.is_draw() {
                us.set_draw();
                them.set_draw();
            } else {
                assert!(!status.is_ongoing());
                us.set_lost();
                them.set_won();
            }

            let name = if status.is_draw() {
                None
            } else {
                Some(them.side().unwrap().name(self.variant))
            };

            self.data.termination = MatchTermination::Normal;
            self.data.reason = self.convert_game_status(name, &status);
            return false;
        }

        // make sure adjudicate is placed after normal termination as it has lower priority
        if self.adjudicate(them, us) {
            return false;
        }

        // Validate connection
        if !self.valid_connection(us, them) {
            return false;
        }

        // Send position
        if !us.engine.position(&self.uci_moves, &self.start_position) {
            self.set_engine_crash_status(us, them);
            return false;
        }

        // Validate connection after position
        if !self.valid_connection(us, them) {
            return false;
        }

        // Send go command — clone time controls to avoid borrow conflict
        let go_cmd = us.engine.get_go(us, them);
        if !us.engine.go(&go_cmd) {
            self.set_engine_crash_status(us, them);
            return false;
        }

        let threshold = {
            let t = us.get_timeout_threshold();
            if t.is_zero() {
                None
            } else {
                Some(t)
            }
        };

        // Wait for bestmove

        let t0 = Instant::now();
        let engine_status = us.engine.read_engine("bestmove", threshold);
        let elapsed_ms = t0.elapsed().as_millis() as i64;

        if crate::is_stop() {
            self.data.termination = MatchTermination::Interrupt;
            return false;
        }

        if engine_status.is_err() {
            self.set_engine_crash_status(us, them);
            return false;
        }

        // Validate connection after search
        if !self.valid_connection(us, them) {
            return false;
        }

        // Get bestmove
        let best_move = us.engine.bestmove();
        let last_time = us.engine.last_time().as_millis() as i64;
        let latency = elapsed_ms - last_time;

        let timeout = if us.has_time_control() {
            !us.time_control_mut().update_time(elapsed_ms)
        } else {
            false
        };
        let timeleft = if us.has_time_control() {
            us.time_control().get_time_left()
        } else {
            0
        };

        // Handle bestmove result
        let mv_str = match best_move {
            None => {
                // No bestmove line found
                if timeout {
                    self.set_engine_timeout_status(us, them);
                } else {
                    self.set_engine_illegal_move_status(us, them, &None);
                }
                return false;
            }
            Some(BestMoveResult::Win) => {
                // USI: Engine declares a win (e.g., delivered checkmate)
                // The engine that played the previous move claims to have won
                // This is typically used when the engine realizes it already won
                self.add_move_data(us, elapsed_ms, latency, timeleft, true);
                us.set_won();
                them.set_lost();
                let stm = self.side_to_move();
                let name = stm.name(self.variant);
                self.data.termination = MatchTermination::Normal;
                self.data.reason = format!("{}{}", name, ENGINE_WIN_MSG);
                return false;
            }
            Some(BestMoveResult::Resign) => {
                // USI: Engine resigns
                self.add_move_data(us, elapsed_ms, latency, timeleft, true);
                us.set_lost();
                them.set_won();
                let stm = self.side_to_move();
                let name = stm.name(self.variant);
                self.data.termination = MatchTermination::Normal;
                self.data.reason = format!("{}{}", name, ENGINE_RESIGN_MSG);
                return false;
            }
            Some(BestMoveResult::Move(mv)) => mv,
        };

        // Validate the move is legal using the game
        let Some(game_move) = self.game.parse_move(&mv_str) else {
            // Illegal move
            self.add_move_data(us, elapsed_ms, latency, timeleft, false);
            self.set_engine_illegal_move_status(us, them, &Some(mv_str));
            return false;
        };

        if timeout {
            self.add_move_data(us, elapsed_ms, latency, timeleft, true);
            self.set_engine_timeout_status(us, them);
            return false;
        }

        // Record move data
        self.add_move_data(us, elapsed_ms, latency, timeleft, true);

        // Verify PV lines from engine output
        self.verify_pv_lines(us, self.test_env, self.check_mate_pvs);

        // Apply the move to the game (this also updates hash history for repetition detection)
        self.game.make_move(game_move.as_ref());
        self.uci_moves.push(mv_str);

        // Update adjudication trackers with score
        let stm_after = self.side_to_move();
        if let Ok(score) = us.engine.last_score() {
            self.draw_tracker
                .update(&score, self.half_move_clock() as i32);
            self.resign_tracker.update(&score, stm_after.opposite());
        } else {
            self.draw_tracker.invalidate();
            self.resign_tracker.invalidate(stm_after.opposite());
        }

        self.maxmoves_tracker.update();

        true
    }

    /// Check adjudication conditions. Returns true if the game is adjudicated.
    fn adjudicate(&mut self, us: &mut Player, them: &mut Player) -> bool {
        // TB adjudication (Syzygy tablebase probing)
        match self.tb_tracker.adjudicate(&self.game) {
            TbAdjudicationResult::Win => {
                us.set_lost();
                them.set_won();
                let stm = self.side_to_move();
                let name = stm.name(self.variant);
                self.data.termination = MatchTermination::Adjudication;
                self.data.reason = format!("{}{}", name, ADJUDICATION_TB_WIN_MSG);
                return true;
            }
            TbAdjudicationResult::Loss => {
                us.set_won();
                them.set_lost();
                let stm = self.side_to_move();
                let name = stm.opposite().name(self.variant);
                self.data.termination = MatchTermination::Adjudication;
                self.data.reason = format!("{}{}", name, ADJUDICATION_TB_WIN_MSG);
                return true;
            }
            TbAdjudicationResult::Draw => {
                us.set_draw();
                them.set_draw();
                self.data.termination = MatchTermination::Adjudication;
                self.data.reason = ADJUDICATION_TB_DRAW_MSG.to_string();
                return true;
            }
            TbAdjudicationResult::None => {
                // No TB adjudication, continue with other checks
            }
        }

        // Resign adjudication
        let resign_enabled = self.resign_tracker.resignable();

        if resign_enabled {
            if let Ok(score) = us.engine.last_score() {
                if score.value < 0 {
                    us.set_lost();
                    them.set_won();

                    let stm = self.side_to_move();
                    let name = stm.name(self.variant);
                    self.data.termination = MatchTermination::Adjudication;
                    self.data.reason = format!("{}{}", name, ADJUDICATION_WIN_MSG);
                    return true;
                }
            }
        }

        // Draw adjudication
        if self.draw_tracker.adjudicatable(self.full_move_number()) {
            us.set_draw();
            them.set_draw();
            self.data.termination = MatchTermination::Adjudication;
            self.data.reason = ADJUDICATION_MSG.to_string();
            return true;
        }

        // Max moves adjudication
        if self.maxmoves_tracker.max_moves_reached() {
            us.set_draw();
            them.set_draw();
            self.data.termination = MatchTermination::Adjudication;
            self.data.reason = ADJUDICATION_MSG.to_string();
            return true;
        }

        false
    }

    /// Validate that the engine is still connected and responsive.
    fn valid_connection(&mut self, us: &mut Player, them: &mut Player) -> bool {
        let is_ready = us.engine.isready(Some(UciEngine::get_ping_time()));

        if is_ready.is_timeout() {
            self.set_engine_stall_status(us, them);
            return false;
        }

        if is_ready.is_err() {
            self.set_engine_crash_status(us, them);
            return false;
        }

        true
    }

    fn set_engine_crash_status(&mut self, loser: &mut Player, winner: &mut Player) {
        loser.set_lost();
        winner.set_won();
        self.stall_or_disconnect = true;

        let stm = self.side_to_move();
        let name = stm.name(self.variant);

        if crate::is_stop() {
            self.data.termination = MatchTermination::Interrupt;
            self.data.reason = INTERRUPTED_MSG.to_string();
        } else {
            self.data.termination = MatchTermination::Disconnect;
            self.data.reason = format!("{}{}", name, DISCONNECT_MSG);
            log_warn!("Engine {} disconnects", loser.engine.config().name);
        }
    }

    fn set_engine_stall_status(&mut self, loser: &mut Player, winner: &mut Player) {
        loser.set_lost();
        winner.set_won();
        self.stall_or_disconnect = true;

        let stm = self.side_to_move();
        let name = stm.name(self.variant);
        self.data.termination = MatchTermination::Stall;
        self.data.reason = format!("{}{}", name, STALL_MSG);

        log_warn!("Engine {} stalls", loser.engine.config().name);
    }

    fn set_engine_timeout_status(&mut self, loser: &mut Player, winner: &mut Player) {
        loser.set_lost();
        winner.set_won();

        let stm = self.side_to_move();
        let name = stm.name(self.variant);
        self.data.termination = MatchTermination::Timeout;
        self.data.reason = format!("{}{}", name, TIMEOUT_MSG);

        log_warn!("Engine {} loses on time", loser.engine.config().name);

        // we send a stop command to the engine to prevent it from thinking
        // and wait for a bestmove to appear
        loser.engine.write_engine("stop");

        if !loser.engine.output_includes_bestmove() {
            let _ = loser
                .engine
                .read_engine("bestmove", Some(Duration::from_secs(1) * 10));
        }
    }

    fn set_engine_illegal_move_status(
        &mut self,
        loser: &mut Player,
        winner: &mut Player,
        best_move: &Option<String>,
    ) {
        loser.set_lost();
        winner.set_won();

        let stm = self.side_to_move();
        let name = stm.name(self.variant);
        self.data.termination = MatchTermination::IllegalMove;
        self.data.reason = format!("{}{}", name, ILLEGAL_MSG);

        let mv = best_move.as_deref().unwrap_or("<none>");
        log_warn!(
            "Warning; Illegal move {} played by {}",
            mv,
            loser.engine.config().name
        );
    }

    /// Record move data from the engine's output.
    fn add_move_data(
        &mut self,
        player: &Player,
        elapsed_ms: i64,
        latency: i64,
        timeleft: i64,
        legal: bool,
    ) {
        let mv = match player.engine.bestmove() {
            Some(BestMoveResult::Move(m)) => m,
            Some(BestMoveResult::Win) => "win".to_string(),
            Some(BestMoveResult::Resign) => "resign".to_string(),
            None => "<none>".to_string(),
        };

        let mut move_data = MoveData {
            r#move: self.game.parse_move(&mv),
            score_string: "0.00".to_string(),
            elapsed_millis: elapsed_ms,
            legal,
            ..Default::default()
        };

        // Extract score info from engine output
        if let Ok(score) = player.engine.last_score() {
            move_data.score = score.value;
            move_data.score_string = Self::convert_score_to_string(score);
        }

        move_data.latency = latency;
        move_data.timeleft = timeleft;

        // Extract additional info data (depth, seldepth, nodes, nps, hashfull, tbhits, pv)
        let info_data = player.engine.last_info_data();
        move_data.depth = info_data.depth;
        move_data.seldepth = info_data.seldepth;
        move_data.nodes = info_data.nodes;
        move_data.nps = info_data.nps;
        move_data.hashfull = info_data.hashfull;
        move_data.tbhits = info_data.tbhits;
        move_data.pv = info_data.pv;

        self.data.moves.push(move_data);
    }

    /// Convert a numeric score to a human-readable string.
    pub fn convert_score_to_string(score: Score) -> String {
        match score.score_type {
            ScoreType::Cp => {
                let normalized = (score.value.unsigned_abs() as f64) / 100.0;
                let sign = if score.value > 0 {
                    "+"
                } else if score.value < 0 {
                    "-"
                } else {
                    ""
                };
                format!("{}{:.2}", sign, normalized)
            }
            ScoreType::Mate => {
                let plies = if score.value > 0 {
                    score.value as u64 * 2 - 1
                } else {
                    ((-score.value) as u64) * 2
                };
                let sign = if score.value > 0 { "+" } else { "-" };
                format!("{}M{}", sign, plies)
            }
            ScoreType::Err => "ERR".to_string(),
        }
    }

    /// Verify PV lines from engine output for illegal moves or continuation after game over.
    fn verify_pv_lines(&self, us: &Player, test_env: bool, check_mate_pvs: bool) {
        let engine_name = &us.engine.config().name;

        // if not chess skip
        if !(self.variant == VariantType::Standard || self.variant == VariantType::Frc) {
            return;
        }

        let info_lines = us.engine.get_info_lines();

        for line in &info_lines {
            self.verify_single_pv_line(&line.line, engine_name, test_env, check_mate_pvs);
        }

        // Finally check if the final PV matches bestmove
        self.verify_bestmove_matches_pv(us, test_env, &info_lines);
    }

    /// Verify that bestmove matches the beginning of the last PV.
    fn verify_bestmove_matches_pv(
        &self,
        us: &Player,
        test_env: bool,
        info_lines: &[&crate::engine::process::Line],
    ) {
        let engine_name = &us.engine.config().name;

        let best_move = match us.engine.bestmove() {
            Some(BestMoveResult::Move(mv)) => mv,
            _ => return,
        };

        // Skip the check if the final score is upperbound/lowerbound
        let Some(last_info) = info_lines.last() else {
            return;
        };

        let is_bound = UciEngine::is_bound(&last_info.line);
        let pv = UciEngine::extract_pv_from_info(&last_info.line);

        if is_bound {
            return;
        }

        let Some(pv_ref) = pv.as_ref() else {
            return;
        };

        if pv_ref.is_empty() {
            return;
        }

        if best_move != pv_ref[0] {
            let warning = format!(
                "Warning; Bestmove does not match beginning of last PV - move {} from {}",
                best_move, engine_name
            );
            self.log_pv_warning(&warning, &last_info.line, engine_name, test_env);
        }
    }

    /// Verify a single PV line from engine output.
    fn verify_single_pv_line(
        &self,
        info: &str,
        engine_name: &str,
        test_env: bool,
        check_mate_pvs: bool,
    ) {
        // Extract PV from info line
        let pv = match UciEngine::extract_pv_from_info(info) {
            Some(pv) if !pv.is_empty() => pv,
            _ => return,
        };

        // Clone the current game state to simulate the PV
        let mut game = self.game.clone();

        for move_str in &pv {
            // Check if game is already over
            let (reason, is_over) = is_game_over_simple(&game);

            if is_over {
                let warning = match reason {
                    GameOverReason::Repetition => {
                        format!(
                            "Warning; PV continues after threefold repetition - move {} from {}",
                            move_str, engine_name
                        )
                    }
                    GameOverReason::FiftyMoveRule => {
                        format!(
                            "Warning; PV continues after fifty-move rule - move {} from {}",
                            move_str, engine_name
                        )
                    }
                    GameOverReason::Checkmate => {
                        format!(
                            "Warning; PV continues after checkmate - move {} from {}",
                            move_str, engine_name
                        )
                    }
                    GameOverReason::Stalemate => {
                        format!(
                            "Warning; PV continues after stalemate - move {} from {}",
                            move_str, engine_name
                        )
                    }
                    _ => format!(
                        "Warning; PV continues after game over - move {} from {}",
                        move_str, engine_name
                    ),
                };

                self.log_pv_warning(&warning, info, engine_name, test_env);
                return;
            }

            // Try to parse and make the move
            let game_move = match game.parse_move(move_str) {
                Some(m) => m,
                None => {
                    let warning = format!(
                        "Warning; Illegal PV move - move {} from {}",
                        move_str, engine_name
                    );
                    self.log_pv_warning(&warning, info, engine_name, test_env);
                    return;
                }
            };

            // Check if move is legal
            if !game.make_move(game_move.as_ref()) {
                let warning = format!(
                    "Warning; Illegal PV move - move {} from {}",
                    move_str, engine_name
                );
                self.log_pv_warning(&warning, info, engine_name, test_env);
                return;
            }
        }

        // For mate scores, check correct length of PV
        if !check_mate_pvs {
            return;
        }

        let score = match UciEngine::get_score(info) {
            Ok(s) => s,
            Err(_) => return,
        };

        let is_bound = UciEngine::is_bound(info);

        if score.score_type != ScoreType::Mate || is_bound {
            return;
        }

        let score_value = score.value;
        let plies = if score_value > 0 {
            (score_value * 2 - 1) as usize
        } else {
            ((-score_value) * 2) as usize
        };

        let mut warning = String::new();

        if pv.len() < plies {
            warning = format!("Warning; Incomplete mating PV - from {}", engine_name);
        } else if pv.len() > plies {
            warning = format!("Warning; Too long mating PV - from {}", engine_name);
        } else {
            // Check if the position after the PV is checkmate
            let status = game.status();
            if !status.is_game_over() || status.reason != GameOverReason::Checkmate {
                warning = format!(
                    "Warning; Mating PV does not end with checkmate - from {}",
                    engine_name
                );
            }
        }

        if !warning.is_empty() {
            self.log_pv_warning(&warning, info, engine_name, test_env);
        }
    }

    /// Log a PV warning with all relevant context.
    fn log_pv_warning(&self, warning: &str, info: &str, _engine_name: &str, test_env: bool) {
        let separator = if test_env { " :: " } else { "\n" };
        let position_str = if self.start_position == "startpos" {
            "startpos".to_string()
        } else {
            format!("fen {}", self.start_position)
        };
        let moves_str = self.uci_moves.join(" ");

        let full_message = format!(
            "{1}{0}Info; {2}{0}Position; {3}{0}Moves; {4}",
            separator, warning, info, position_str, moves_str
        );

        log_warn!("{}", full_message);
    }

    /// Convert a GameStatus to a human-readable reason string.
    fn convert_game_status(&self, name: Option<&str>, status: &GameStatus) -> String {
        status.reason.message(name, self.variant)
    }

    /// Get the match data after the game has finished.
    pub fn get(&self) -> &MatchData {
        &self.data
    }

    /// Whether the game ended due to a stall or disconnect.
    pub fn is_stall_or_disconnect(&self) -> bool {
        self.stall_or_disconnect
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{engine::uci_engine::Score, game::GameOverReason};

    #[test]
    fn test_convert_score_cp_positive() {
        let score = Score {
            value: 150,
            score_type: ScoreType::Cp,
        };

        assert_eq!(Match::convert_score_to_string(score), "+1.50");
    }

    #[test]
    fn test_convert_score_cp_negative() {
        let score = Score {
            value: -75,
            score_type: ScoreType::Cp,
        };
        assert_eq!(Match::convert_score_to_string(score), "-0.75");
    }

    #[test]
    fn test_convert_score_cp_zero() {
        let score = Score {
            value: 0,
            score_type: ScoreType::Cp,
        };
        assert_eq!(Match::convert_score_to_string(score), "0.00");
    }

    #[test]
    fn test_convert_score_mate_positive() {
        // Mate in 3 = 5 plies
        let score = Score {
            value: 3,
            score_type: ScoreType::Mate,
        };
        assert_eq!(Match::convert_score_to_string(score), "+M5");
    }

    #[test]
    fn test_convert_score_mate_negative() {
        // Mated in 2 = 4 plies
        let score = Score {
            value: -2,
            score_type: ScoreType::Mate,
        };
        assert_eq!(Match::convert_score_to_string(score), "-M4");
    }

    #[test]
    fn test_convert_score_err() {
        let score = Score {
            value: 0,
            score_type: ScoreType::Err,
        };
        assert_eq!(Match::convert_score_to_string(score), "ERR");
    }

    #[test]
    fn test_side_from_board_default() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::startpos();
        let m = Match::new(opening, &tc);
        assert_eq!(m.side_to_move(), Side::White);
    }

    #[test]
    fn test_side_from_board_black() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1".to_string()),
            Vec::new(),
        );
        let m = Match::new(opening, &tc);
        assert_eq!(m.side_to_move(), Side::Black);
    }

    #[test]
    fn test_game_status_conversion() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::startpos();
        let m = Match::new(opening, &tc);

        let status = GameStatus {
            reason: GameOverReason::Checkmate,
        };
        assert_eq!(
            m.convert_game_status(Some(Side::White.name(VariantType::Standard)), &status),
            "White mates"
        );

        let status = GameStatus {
            reason: GameOverReason::Stalemate,
        };
        assert_eq!(
            m.convert_game_status(Some(&""), &status),
            "Draw by stalemate"
        );

        let status = GameStatus {
            reason: GameOverReason::FiftyMoveRule,
        };
        assert_eq!(
            m.convert_game_status(Some(&""), &status),
            "Draw by fifty moves rule"
        );
    }

    #[test]
    fn test_game_over_checkmate() {
        let tc = crate::types::tournament::TournamentConfig::default();
        // Fool's mate — white is in checkmate
        let opening = Opening::new(
            Some("rnb1kbnr/pppp1ppp/4p3/8/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3".to_string()),
            Vec::new(),
        );
        let m = Match::new(opening, &tc);
        let status = m.game.status();
        assert_eq!(status.reason, GameOverReason::Checkmate);
    }

    #[test]
    fn test_opening_book_moves_applied() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()),
            vec!["e2e4".to_string(), "e7e5".to_string()],
        );
        let m = Match::new(opening, &tc);
        assert_eq!(m.game.ply_count(), 2);
        assert_eq!(m.uci_moves.len(), 2);
        assert_eq!(m.side_to_move(), Side::White);
    }

    #[test]
    fn test_resign_adjudication_names_correct_winner() {
        // In adjudicate(), `us` is the engine that just moved (opposite of side to move),
        // because play_move() calls adjudicate(them, us) with swapped args.
        // When `us` reports a negative score, it means `us` is losing and the side
        // to move (stm) is winning. The reason string should name stm as the winner.
        use crate::types::engine_config::EngineConfiguration;

        let mut tc = crate::types::tournament::TournamentConfig::default();
        tc.resign.enabled = true;
        tc.resign.move_count = 0; // immediately resignable

        // White to move at startpos
        let opening = Opening::startpos();
        let mut m = Match::new(opening, &tc);
        assert_eq!(m.side_to_move(), Side::White);

        let cfg = EngineConfiguration::default();
        let mut engine_us = UciEngine::new(&cfg, false).expect("Failed to create UciEngine");
        let mut engine_them = UciEngine::new(&cfg, false).expect("Failed to create UciEngine");

        // `us` in adjudicate is the engine that just moved (Black, since stm is White).
        // Black reports a losing score → White (stm) is the winner.
        engine_us.inject_output_line("info depth 10 score cp -500 pv e2e4");

        let mut us = Player::new(&mut engine_us);
        let mut them = Player::new(&mut engine_them);

        let adjudicated = m.adjudicate(&mut us, &mut them);
        assert!(adjudicated);
        assert_eq!(m.data.termination, MatchTermination::Adjudication);
        // stm is White, and stm is the winner
        assert_eq!(m.data.reason, "White wins by adjudication");
    }

    #[test]
    fn test_full_move_number() {
        let tc = crate::types::tournament::TournamentConfig::default();

        // Startpos: ply 0 → full move 1
        let opening = Opening::startpos();
        let m = Match::new(opening, &tc);
        assert_eq!(m.ply_count(), 0);
        assert_eq!(m.full_move_number(), 1);

        // After 1 ply (e2e4): ply 1 → full move 1
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()),
            vec!["e2e4".to_string()],
        );
        let m = Match::new(opening, &tc);
        assert_eq!(m.ply_count(), 1);
        assert_eq!(m.full_move_number(), 1);

        // After 2 plies (e2e4, e7e5): ply 2 → full move 2
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()),
            vec!["e2e4".to_string(), "e7e5".to_string()],
        );
        let m = Match::new(opening, &tc);
        assert_eq!(m.ply_count(), 2);
        assert_eq!(m.full_move_number(), 2);

        // After 3 plies: ply 3 → full move 2
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()),
            vec!["e2e4".to_string(), "e7e5".to_string(), "g1f3".to_string()],
        );
        let m = Match::new(opening, &tc);
        assert_eq!(m.ply_count(), 3);
        assert_eq!(m.full_move_number(), 2);
    }

    #[test]
    fn test_draw_adjudication_uses_full_move_number() {
        // Draw adjudication checks full_move_number() against DrawAdjudication::move_number.
        // With move_number=3, adjudication should only trigger at full move 3+ (ply 4+).
        use crate::types::engine_config::EngineConfiguration;

        let mut tc = crate::types::tournament::TournamentConfig::default();
        tc.draw.enabled = true;
        tc.draw.move_count = 0; // immediately adjudicatable (no score tracking needed)
        tc.draw.move_number = 3; // only adjudicate at full move 3 or later

        let opening = Opening::startpos();
        let mut m = Match::new(opening, &tc);
        let cfg = EngineConfiguration::default();

        let mut engine_us = UciEngine::new(&cfg, false).expect("Failed to create UciEngine");
        let mut engine_them = UciEngine::new(&cfg, false).expect("Failed to create UciEngine");
        let mut us = Player::new(&mut engine_us);
        let mut them = Player::new(&mut engine_them);

        // Ply 0 (full move 1): should NOT adjudicate
        assert_eq!(m.full_move_number(), 1);
        assert!(!m.adjudicate(&mut us, &mut them));

        // Play e2e4 → ply 1 (full move 1): should NOT adjudicate
        assert!(m.game.make_uci_move("e2e4"));
        assert_eq!(m.full_move_number(), 1);
        assert!(!m.adjudicate(&mut us, &mut them));

        // Play e7e5 → ply 2 (full move 2): should NOT adjudicate
        assert!(m.game.make_uci_move("e7e5"));
        assert_eq!(m.full_move_number(), 2);
        assert!(!m.adjudicate(&mut us, &mut them));

        // Play g1f3 → ply 3 (full move 2): should NOT adjudicate
        assert!(m.game.make_uci_move("g1f3"));
        assert_eq!(m.full_move_number(), 2);
        assert!(!m.adjudicate(&mut us, &mut them));

        // Play b8c6 → ply 4 (full move 3): should adjudicate
        assert!(m.game.make_uci_move("b8c6"));
        assert_eq!(m.full_move_number(), 3);
        assert!(m.adjudicate(&mut us, &mut them));
        assert_eq!(m.data.termination, MatchTermination::Adjudication);
        assert_eq!(m.data.reason, "Draw by adjudication");
    }

    #[test]
    fn test_adjudicate_swapped_args() {
        // play_move() calls adjudicate(them, us) — with swapped args.
        // This means inside adjudicate, `us` is the engine that just moved,
        // not the side to move. Verify both resign and draw adjudication
        // produce correct results with this convention.
        use crate::types::engine_config::EngineConfiguration;

        let mut tc = crate::types::tournament::TournamentConfig::default();
        tc.resign.enabled = true;
        tc.resign.move_count = 0;

        // Black to move (after e2e4)
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()),
            vec!["e2e4".to_string()],
        );
        let mut m = Match::new(opening, &tc);
        assert_eq!(m.side_to_move(), Side::Black);

        let cfg = EngineConfiguration::default();
        // `us` in adjudicate = the engine that just moved = White
        // White reports losing score → Black (stm) wins
        let mut engine_us = UciEngine::new(&cfg, false).expect("Failed to create UciEngine");
        let mut engine_them = UciEngine::new(&cfg, false).expect("Failed to create UciEngine");
        engine_us.inject_output_line("info depth 10 score cp -600 pv e7e5");

        let mut us = Player::new(&mut engine_us);
        let mut them = Player::new(&mut engine_them);

        let adjudicated = m.adjudicate(&mut us, &mut them);
        assert!(adjudicated);
        assert_eq!(m.data.reason, "Black wins by adjudication");
    }

    #[test]
    fn test_threefold_repetition_detection() {
        let tc = crate::types::tournament::TournamentConfig::default();
        // Set up a position where we can create threefold repetition
        // Knights bouncing back and forth: Ng1-f3, Ng8-f6, Nf3-g1, Nf6-g8, Ng1-f3, Ng8-f6, Nf3-g1, Nf6-g8
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()),
            vec![
                "g1f3".to_string(),
                "g8f6".to_string(),
                "f3g1".to_string(),
                "f6g8".to_string(),
                "g1f3".to_string(),
                "g8f6".to_string(),
                "f3g1".to_string(),
                "f6g8".to_string(),
            ],
        );
        let m = Match::new(opening, &tc);
        // After these moves, the starting position should have occurred 3 times:
        // initial, after move 4, after move 8
        let status = m.game.status();
        assert_eq!(status.reason, GameOverReason::Repetition);
    }

    #[test]
    fn test_is_game_over_simple_ongoing() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::startpos();
        let m = Match::new(opening, &tc);

        let (reason, is_over) = is_game_over_simple(&m.game);
        assert!(!is_over);
        assert_eq!(reason, GameOverReason::None);
    }

    #[test]
    fn test_is_game_over_simple_checkmate() {
        let tc = crate::types::tournament::TournamentConfig::default();
        // Fool's mate position - white is checkmated
        let opening = Opening::new(
            Some("rnb1kbnr/pppp1ppp/4p3/8/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3".to_string()),
            Vec::new(),
        );
        let m = Match::new(opening, &tc);

        let (reason, is_over) = is_game_over_simple(&m.game);
        assert!(is_over);
        assert_eq!(reason, GameOverReason::Checkmate);
    }

    #[test]
    fn test_is_game_over_simple_stalemate() {
        // This test verifies the is_game_over_simple function correctly detects stalemate
        // We'll use a known stalemate position from a real game
        let tc = crate::types::tournament::TournamentConfig::default();

        // Classic stalemate: Black king on h8, white queen on f7, white king on f6
        // Black has no legal moves and is not in check
        let opening = Opening::new(
            Some("7k/5Q2/5K2/8/8/8/8/8 b - - 0 1".to_string()),
            Vec::new(),
        );
        let m = Match::new(opening, &tc);

        let (reason, is_over) = is_game_over_simple(&m.game);
        assert!(is_over, "Expected game to be over (stalemate)");
        assert_eq!(
            reason,
            GameOverReason::Stalemate,
            "Expected stalemate reason"
        );
    }
}
