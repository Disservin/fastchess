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

use std::time::Instant;

use crate::engine::process::ProcessResultExt;
use crate::engine::protocol::Protocol;
use crate::engine::uci_engine::{BestMoveResult, Color, ScoreType, UciEngine};
use crate::game::book::Opening;
use crate::game::{GameInstance, GameStatus};
use crate::matchmaking::player::Player;
use crate::types::engine_config::GamePair;
use crate::types::match_data::*;
use crate::types::VariantType;

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

// ── Color helpers ────────────────────────────────────────────────────────────

fn color_name(color: Color, variant: VariantType) -> &'static str {
    let protocol = Protocol::new(variant);
    match color {
        Color::White => protocol.first_player_name(),
        Color::Black => protocol.second_player_name(),
    }
}

fn opposite(color: Color) -> Color {
    match color {
        Color::White => Color::Black,
        Color::Black => Color::White,
    }
}

/// Convert from game::Color to engine::uci_engine::Color
fn game_color_to_uci_color(c: crate::game::Color) -> Color {
    match c {
        crate::game::Color::First => Color::White,
        crate::game::Color::Second => Color::Black,
    }
}

// ── Match ────────────────────────────────────────────────────────────────────

/// Orchestrates a single game between two UCI engines.
pub struct Match {
    #[allow(dead_code)]
    opening: Opening,
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

        let fen_str = if opening.fen_epd.is_none() {
            GameInstance::default_fen(variant).to_string()
        } else {
            opening.fen_epd.clone().unwrap()
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

        Self {
            opening,
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
        }
    }

    /// Current side to move (from the game).
    fn side_to_move(&self) -> Color {
        game_color_to_uci_color(self.game.side_to_move())
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
    pub fn start(&mut self, white: &mut UciEngine, black: &mut UciEngine, cpus: Option<&[i32]>) {
        let mut white_player = Player::new(white);
        let mut black_player = Player::new(black);

        // Start engines
        if let Err(e) = white_player.engine.start(cpus) {
            if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
                return;
            }
            crate::STOP.store(true, std::sync::atomic::Ordering::Relaxed);
            crate::ABNORMAL_TERMINATION.store(true, std::sync::atomic::Ordering::Relaxed);
            log::error!(
                "Fatal; {} engine startup failure: \"{}\"",
                white_player.engine.config().name,
                e
            );
            return;
        }

        if let Err(e) = black_player.engine.start(cpus) {
            if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
                return;
            }
            crate::STOP.store(true, std::sync::atomic::Ordering::Relaxed);
            crate::ABNORMAL_TERMINATION.store(true, std::sync::atomic::Ordering::Relaxed);
            log::error!(
                "Fatal; {} engine startup failure: \"{}\"",
                black_player.engine.config().name,
                e
            );
            return;
        }

        if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
            return;
        }

        // Refresh UCI
        if !white_player.engine.refresh_uci() {
            self.set_engine_crash_status(&mut white_player, &mut black_player);
        }

        if !black_player.engine.refresh_uci() {
            self.set_engine_crash_status(&mut black_player, &mut white_player);
        }

        // Check connection
        self.valid_connection(&mut white_player, &mut black_player);

        let start_time = Instant::now();

        if self.data.termination == MatchTermination::None {
            let white_first = self.side_to_move() == Color::White;
            self.game_loop(&mut white_player, &mut black_player, white_first);
        }

        let elapsed = start_time.elapsed();

        self.data.variant = self.variant;
        self.data.end_time = crate::core::datetime_iso();
        self.data.duration = crate::core::duration_string(elapsed);

        self.data.players = GamePair::new(
            PlayerInfo {
                config: white_player.engine.config().clone(),
                result: white_player.result(),
            },
            PlayerInfo {
                config: black_player.engine.config().clone(),
                result: black_player.result(),
            },
        );
    }

    /// The core game loop.
    fn game_loop<'a>(&mut self, white: &mut Player<'a>, black: &mut Player<'a>, white_first: bool) {
        loop {
            if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
                self.data.termination = MatchTermination::Interrupt;
                break;
            }

            let cont = if white_first {
                self.play_move(white, black)
            } else {
                self.play_move(black, white)
            };
            if !cont {
                break;
            }

            if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
                self.data.termination = MatchTermination::Interrupt;
                break;
            }

            let cont = if white_first {
                self.play_move(black, white)
            } else {
                self.play_move(white, black)
            };
            if !cont {
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
            } else if status.winner.is_some() {
                // The side to move lost (they got checkmated)
                us.set_lost();
                them.set_won();
            }

            self.data.termination = MatchTermination::Normal;
            self.data.reason = self.convert_game_status(&status);
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
        let our_tc = us.time_control().clone();
        let their_tc = them.time_control().clone();
        let stm = self.side_to_move();
        if !us.engine.go(&our_tc, &their_tc, stm) {
            self.set_engine_crash_status(us, them);
            return false;
        }

        // Wait for bestmove
        let t0 = Instant::now();
        let threshold = {
            let t = us.get_timeout_threshold();
            if t.is_zero() {
                None
            } else {
                Some(t)
            }
        };
        let engine_status = us.engine.read_engine("bestmove", threshold);
        let t1 = Instant::now();

        let elapsed_ms = t1.duration_since(t0).as_millis() as i64;

        if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
            self.data.termination = MatchTermination::Interrupt;
            return false;
        }

        if !engine_status.is_ok() {
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
                let name = color_name(stm, self.variant);
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
                let name = color_name(stm, self.variant);
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

        // Apply the move to the game (this also updates hash history for repetition detection)
        self.game.make_move(game_move.as_ref());
        self.uci_moves.push(mv_str);

        // Update adjudication trackers with score
        let stm_after = self.side_to_move();
        if let Ok(score) = us.engine.last_score() {
            self.draw_tracker
                .update(&score, self.half_move_clock() as i32);
            self.resign_tracker.update(&score, opposite(stm_after));
        } else {
            self.draw_tracker.invalidate();
            self.resign_tracker.invalidate(opposite(stm_after));
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
                let name = color_name(stm, self.variant);
                self.data.termination = MatchTermination::Adjudication;
                self.data.reason = format!("{}{}", name, ADJUDICATION_TB_WIN_MSG);
                return true;
            }
            TbAdjudicationResult::Loss => {
                us.set_won();
                them.set_lost();
                let stm = self.side_to_move();
                let name = color_name(opposite(stm), self.variant);
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
                    let name = color_name(stm, self.variant);
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
        let name = color_name(stm, self.variant);

        if crate::STOP.load(std::sync::atomic::Ordering::Relaxed) {
            self.data.termination = MatchTermination::Interrupt;
            self.data.reason = INTERRUPTED_MSG.to_string();
        } else {
            self.data.termination = MatchTermination::Disconnect;
            self.data.reason = format!("{}{}", name, DISCONNECT_MSG);
        }

        log::warn!("Engine {} disconnects", loser.engine.config().name);
    }

    fn set_engine_stall_status(&mut self, loser: &mut Player, winner: &mut Player) {
        loser.set_lost();
        winner.set_won();
        self.stall_or_disconnect = true;

        let stm = self.side_to_move();
        let name = color_name(stm, self.variant);
        self.data.termination = MatchTermination::Stall;
        self.data.reason = format!("{}{}", name, STALL_MSG);

        log::warn!("Engine {} stalls", loser.engine.config().name);
    }

    fn set_engine_timeout_status(&mut self, loser: &mut Player, winner: &mut Player) {
        loser.set_lost();
        winner.set_won();

        let stm = self.side_to_move();
        let name = color_name(stm, self.variant);
        self.data.termination = MatchTermination::Timeout;
        self.data.reason = format!("{}{}", name, TIMEOUT_MSG);

        log::warn!("Engine {} loses on time", loser.engine.config().name);

        // Send stop and wait for bestmove
        loser.engine.write_engine("stop");
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
        let name = color_name(stm, self.variant);
        self.data.termination = MatchTermination::IllegalMove;
        self.data.reason = format!("{}{}", name, ILLEGAL_MSG);

        let mv = best_move.as_deref().unwrap_or("<none>");
        log::warn!(
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
            move_data.score_string =
                Self::convert_score_to_string(score.value as i32, score.score_type);
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
    pub fn convert_score_to_string(score: i32, score_type: ScoreType) -> String {
        match score_type {
            ScoreType::Cp => {
                let normalized = (score.unsigned_abs() as f64) / 100.0;
                let sign = if score > 0 {
                    "+"
                } else if score < 0 {
                    "-"
                } else {
                    ""
                };
                format!("{}{:.2}", sign, normalized)
            }
            ScoreType::Mate => {
                let plies = if score > 0 {
                    score as u64 * 2 - 1
                } else {
                    ((-score) as u64) * 2
                };
                let sign = if score > 0 { "+" } else { "-" };
                format!("{}M{}", sign, plies)
            }
            ScoreType::Err => "ERR".to_string(),
        }
    }

    /// Convert a GameStatus to a human-readable reason string.
    fn convert_game_status(&self, status: &GameStatus) -> String {
        status.reason.message(status.winner, self.variant)
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
    use crate::game::GameOverReason;

    #[test]
    fn test_convert_score_cp_positive() {
        assert_eq!(Match::convert_score_to_string(150, ScoreType::Cp), "+1.50");
    }

    #[test]
    fn test_convert_score_cp_negative() {
        assert_eq!(Match::convert_score_to_string(-75, ScoreType::Cp), "-0.75");
    }

    #[test]
    fn test_convert_score_cp_zero() {
        assert_eq!(Match::convert_score_to_string(0, ScoreType::Cp), "0.00");
    }

    #[test]
    fn test_convert_score_mate_positive() {
        // Mate in 3 = 5 plies
        assert_eq!(Match::convert_score_to_string(3, ScoreType::Mate), "+M5");
    }

    #[test]
    fn test_convert_score_mate_negative() {
        // Mated in 2 = 4 plies
        assert_eq!(Match::convert_score_to_string(-2, ScoreType::Mate), "-M4");
    }

    #[test]
    fn test_convert_score_err() {
        assert_eq!(Match::convert_score_to_string(0, ScoreType::Err), "ERR");
    }

    #[test]
    fn test_side_from_board_default() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::startpos();
        let m = Match::new(opening, &tc);
        assert_eq!(m.side_to_move(), Color::White);
    }

    #[test]
    fn test_side_from_board_black() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::new(
            Some("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1".to_string()),
            Vec::new(),
        );
        let m = Match::new(opening, &tc);
        assert_eq!(m.side_to_move(), Color::Black);
    }

    #[test]
    fn test_game_status_conversion() {
        let tc = crate::types::tournament::TournamentConfig::default();
        let opening = Opening::startpos();
        let m = Match::new(opening, &tc);

        let status = GameStatus {
            reason: GameOverReason::Checkmate,
            winner: Some(crate::game::Color::First),
        };
        assert_eq!(m.convert_game_status(&status), "White mates");

        let status = GameStatus {
            reason: GameOverReason::Stalemate,
            winner: None,
        };
        assert_eq!(m.convert_game_status(&status), "Draw by stalemate");

        let status = GameStatus {
            reason: GameOverReason::FiftyMoveRule,
            winner: None,
        };
        assert_eq!(m.convert_game_status(&status), "Draw by fifty moves rule");
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
        assert_eq!(status.winner, Some(crate::game::Color::Second));
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
        assert_eq!(m.side_to_move(), Color::White);
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
        assert_eq!(m.side_to_move(), Color::White);

        let cfg = EngineConfiguration::default();
        let mut engine_us = UciEngine::new(&cfg, false);
        let mut engine_them = UciEngine::new(&cfg, false);

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

        let mut engine_us = UciEngine::new(&cfg, false);
        let mut engine_them = UciEngine::new(&cfg, false);
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
        assert_eq!(m.side_to_move(), Color::Black);

        let cfg = EngineConfiguration::default();
        // `us` in adjudicate = the engine that just moved = White
        // White reports losing score → Black (stm) wins
        let mut engine_us = UciEngine::new(&cfg, false);
        let mut engine_them = UciEngine::new(&cfg, false);
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
}
