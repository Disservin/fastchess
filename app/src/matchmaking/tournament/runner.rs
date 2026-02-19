//! Tournament runner — orchestrates a complete tournament of games.
//!
//! Ports `matchmaking/tournament/base/tournament.hpp/cpp` and
//! `matchmaking/tournament/roundrobin/roundrobin.hpp/cpp` from C++.
//!
//! In C++, there's a `BaseTournament` → `RoundRobin` → `Gauntlet` hierarchy.
//! Since `Gauntlet` only differs in the scheduler (which we handle via
//! `SchedulerVariant`), we merge everything into a single `Tournament` struct.

use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use thiserror::Error;

use crate::affinity::AffinityManager;
use crate::core::config;
use crate::core::file_writer::FileWriter;
use crate::engine::cache::{EngineCache, EngineGuard};
use crate::game::book::OpeningBook;
use crate::game::pgn::PgnBuilder;
use crate::matchmaking::match_runner::Match;
use crate::matchmaking::output::{create_output, EngineDisplayInfo};
use crate::matchmaking::scoreboard::{PlayerPairKey, ScoreBoard, StatsMap};
use crate::matchmaking::sprt::{Sprt, SprtModel, SprtResult};
use crate::matchmaking::stats::Stats;
use crate::matchmaking::timeout_tracker::PlayerTracker;
use crate::matchmaking::tournament::schedule::{Pairing, Scheduler, SchedulerVariant};
use crate::types::match_data::*;

#[derive(Error, Debug, Clone)]
pub enum RunGameError {
    #[error("Failed to create first engine: {0}")]
    FirstEngineCreation(String),
    #[error("Failed to create second engine: {0}")]
    SecondEngineCreation(String),
    #[error("Game {0} stalled/disconnected, no recover option set, stopping tournament.")]
    GameStalled(usize),
}

/// Shared state that is cloned into each game-running closure.
///
/// Bundles all `Arc`-wrapped handles needed by workers so that the
/// game-running logic can be a free function rather than a method on
/// `Tournament`.
#[derive(Clone)]
struct SharedState {
    scheduler: Arc<Mutex<Option<Scheduler>>>,
    scoreboard: Arc<ScoreBoard>,
    tracker: Arc<Mutex<PlayerTracker>>,
    output: Arc<Mutex<Box<dyn crate::matchmaking::output::Output + Send>>>,
    /// Mutex that serializes the end-game output block (endGame + scoreboard
    /// update + printResult + printInterval + SPRT check) to avoid interleaved
    /// output from concurrent workers.
    output_mutex: Arc<Mutex<()>>,
    match_count: Arc<std::sync::atomic::AtomicU64>,
    final_matchcount: u64,
    file_writer_pgn: Option<Arc<FileWriter>>,
    file_writer_epd: Option<Arc<FileWriter>>,
    sprt: Arc<Sprt>,
    opening_file: String,
    engine_cache: Arc<EngineCache>,
    affinity_manager: Arc<AffinityManager>,
}

/// A complete tournament runner.
///
/// Handles scheduling, engine management, game execution, result tracking,
/// SPRT monitoring, and file output (PGN/EPD).
pub struct Tournament {
    scoreboard: Arc<ScoreBoard>,
    tracker: Arc<Mutex<PlayerTracker>>,
    output: Arc<Mutex<Box<dyn crate::matchmaking::output::Output + Send>>>,
    output_mutex: Arc<Mutex<()>>,
    sprt: Arc<Sprt>,

    match_count: Arc<std::sync::atomic::AtomicU64>,
    initial_matchcount: u64,
    final_matchcount: u64,

    file_writer_pgn: Option<Arc<FileWriter>>,
    file_writer_epd: Option<Arc<FileWriter>>,

    // The scheduler is behind a mutex because `startNext()` is called from threads
    scheduler: Arc<Mutex<Option<Scheduler>>>,

    // Engine cache — reuses engine processes across games
    engine_cache: Arc<EngineCache>,

    // CPU affinity manager — allocates CPU cores to worker threads
    affinity_manager: Arc<AffinityManager>,
}

impl Tournament {
    /// Create a new tournament from configuration.
    ///
    /// `results` contains any previously saved results (for resumption).
    /// `variant` specifies whether this is a round-robin or gauntlet tournament.
    pub fn new(
        results: &crate::matchmaking::scoreboard::StatsMap,
        variant: SchedulerVariant,
    ) -> Result<Self, String> {
        let tournament_config = config::tournament_config();
        let engine_configs = config::engine_configs();

        // Calculate initial match count from results
        let initial_matchcount: u64 = results
            .values()
            .map(|s| (s.wins + s.losses + s.draws) as u64)
            .sum();

        let scoreboard = ScoreBoard::default();

        let output = create_output(tournament_config.output, tournament_config.report_penta);

        let num_players = engine_configs.len();

        // Create opening book
        let book = OpeningBook::new(
            &tournament_config.opening.file,
            tournament_config.opening.format,
            tournament_config.opening.order,
            tournament_config.opening.plies,
            tournament_config.opening.start,
            tournament_config.rounds,
            tournament_config.games,
            initial_matchcount as usize,
            tournament_config.variant,
        )?;

        // Scheduler now owns the opening book - no need for Box::leak
        let scheduler = Scheduler::new(
            book,
            variant,
            num_players,
            tournament_config.rounds,
            tournament_config.games,
            initial_matchcount as usize,
        );

        let final_matchcount = scheduler.total() as u64;

        let sprt_model =
            SprtModel::from_str(&tournament_config.sprt.model).unwrap_or(SprtModel::Normalized);

        let sprt = Sprt::new(
            tournament_config.sprt.alpha,
            tournament_config.sprt.beta,
            tournament_config.sprt.elo0,
            tournament_config.sprt.elo1,
            sprt_model,
            tournament_config.sprt.enabled,
        );

        // Set up file writers
        let file_writer_pgn = if !tournament_config.pgn.file.is_empty() {
            let append = tournament_config.pgn.append_file || initial_matchcount > 0;
            Some(Arc::new(
                FileWriter::new(
                    &tournament_config.pgn.file,
                    append,
                    tournament_config.pgn.crc,
                )
                .map_err(|e| e.to_string())?,
            ))
        } else {
            None
        };

        let file_writer_epd = if !tournament_config.epd.file.is_empty() {
            let append = tournament_config.epd.append_file || initial_matchcount > 0;
            Some(Arc::new(
                FileWriter::new(&tournament_config.epd.file, append, false)
                    .map_err(|e| e.to_string())?,
            ))
        } else {
            None
        };

        // Initialize Rayon thread pool with the specified concurrency
        rayon::ThreadPoolBuilder::new()
            .num_threads(tournament_config.concurrency)
            .build_global()
            .expect("Failed to initialize Rayon thread pool");

        let engine_cache = Arc::new(EngineCache::new(tournament_config.affinity));

        // Create affinity manager (threads-per-engine = 1 by default)
        let affinity_manager = Arc::new(AffinityManager::new(
            tournament_config.affinity,
            &tournament_config.affinity_cpus,
            1, // tpe: For now, we use 1. In C++, this is getMaxAffinity()
        ));

        Ok(Self {
            scoreboard: Arc::new(scoreboard),
            tracker: Arc::new(Mutex::new(PlayerTracker::new())),
            output: Arc::new(Mutex::new(output)),
            output_mutex: Arc::new(Mutex::new(())),
            sprt: Arc::new(sprt),
            match_count: Arc::new(std::sync::atomic::AtomicU64::new(initial_matchcount)),
            initial_matchcount,
            final_matchcount,
            file_writer_pgn,
            file_writer_epd,
            scheduler: Arc::new(Mutex::new(Some(scheduler))),
            engine_cache,
            affinity_manager,
        })
    }

    /// Start and run the tournament to completion.
    pub fn start(&mut self) {
        log::trace!("Starting tournament...");

        self.create();

        let save_interval = config::tournament_config().autosaveinterval;
        let mut save_iter = self.initial_matchcount + save_interval as u64;

        // Wait for games to finish
        while self.match_count.load(Ordering::Relaxed) < self.final_matchcount && !crate::is_stop()
        {
            if save_interval > 0 && self.match_count.load(Ordering::Relaxed) >= save_iter {
                self.save_json();
                save_iter += save_interval as u64;
            }
            thread::sleep(Duration::from_secs(1));
        }

        // Print final timeout/disconnect stats
        self.print_tracker_stats();
        self.save_json();
    }

    /// Build the `SharedState` bundle for worker closures.
    fn shared_state(&self) -> SharedState {
        SharedState {
            scheduler: Arc::clone(&self.scheduler),
            scoreboard: Arc::clone(&self.scoreboard),
            tracker: Arc::clone(&self.tracker),
            output: Arc::clone(&self.output),
            output_mutex: Arc::clone(&self.output_mutex),
            match_count: Arc::clone(&self.match_count),
            final_matchcount: self.final_matchcount,
            file_writer_pgn: self.file_writer_pgn.clone(),
            file_writer_epd: self.file_writer_epd.clone(),
            sprt: Arc::clone(&self.sprt),
            opening_file: config::tournament_config().opening.file.clone(),
            engine_cache: Arc::clone(&self.engine_cache),
            affinity_manager: Arc::clone(&self.affinity_manager),
        }
    }

    /// Kick off the initial batch of games (one per thread).
    fn create(&mut self) {
        log::trace!("Creating matches...");

        let tournament_config = config::tournament_config();
        let num_threads = tournament_config.concurrency;
        let state = self.shared_state();

        for _ in 0..num_threads {
            let pairing = {
                let mut sched = state.scheduler.lock().unwrap();
                sched.as_mut().and_then(|s| s.next())
            };

            if let Some(pairing) = pairing {
                let state = state.clone();
                rayon::spawn(move || {
                    run_game(pairing, state);
                });
            }
        }
    }

    /// Print timeout/disconnect statistics.
    fn print_tracker_stats(&self) {
        let tracker = self.tracker.lock().unwrap();
        for (name, timeouts, disconnects) in tracker.iter() {
            log::info!("Player: {}", name);
            log::info!("  Timeouts: {}", timeouts);
            log::info!("  Crashed: {}", disconnects);
        }
    }

    /// Save tournament results to JSON for resumption.
    ///
    /// Writes the tournament config, engine configs, and current stats
    /// to `config_name` (default: "config.json").
    fn save_json(&self) {
        log::trace!("Saving results...");

        let tournament_config = config::tournament_config();
        let engine_configs = config::engine_configs();

        // Merge results from scoreboard
        let merged_results = self.merge_results();

        // Build the JSON: tournament config at root, engines + stats as extra fields
        let mut json_val = match serde_json::to_value(tournament_config) {
            Ok(v) => v,
            Err(e) => {
                log::warn!("Failed to serialize tournament config: {}", e);
                return;
            }
        };

        if let Some(obj) = json_val.as_object_mut() {
            obj.insert(
                "engines".to_string(),
                serde_json::to_value(engine_configs).unwrap_or_default(),
            );
            obj.insert(
                "stats".to_string(),
                crate::matchmaking::scoreboard::stats_map_to_json(&merged_results),
            );
        }

        let config_name = &tournament_config.config_name;
        match std::fs::File::create(config_name) {
            Ok(file) => {
                let writer = std::io::BufWriter::new(file);
                if let Err(e) = serde_json::to_writer_pretty(writer, &json_val) {
                    log::warn!("Failed to write JSON to {}: {}", config_name, e);
                } else {
                    log::trace!("Saved results to {}", config_name);
                }
            }
            Err(e) => {
                log::warn!("Failed to create file {}: {}", config_name, e);
            }
        }
    }

    /// Merge all per-pair stats from the scoreboard into a StatsMap.
    ///
    /// This mirrors the C++ `merged_results` lambda that iterates all
    /// engine pairs and collects their stats.
    fn merge_results(&self) -> StatsMap {
        let engine_configs = config::engine_configs();
        let mut map = StatsMap::new();
        let mut seen = std::collections::HashSet::new();

        for engine in engine_configs.iter() {
            for opponent in engine_configs.iter() {
                if engine.name == opponent.name {
                    continue;
                }

                let pair = PlayerPairKey::new(&engine.name, &opponent.name);
                let reverse = PlayerPairKey::new(&opponent.name, &engine.name);

                if seen.contains(&pair) || seen.contains(&reverse) {
                    continue;
                }

                let stats = self.scoreboard.get_stats(&engine.name, &opponent.name);
                map.insert(pair.clone(), stats);
                seen.insert(pair);
            }
        }

        map
    }
}

/// Check whether the score interval should trigger at the current match count.
///
/// C++: `(match_count_ + 1) % cfg.scoreinterval == 0`
/// Called before incrementing match_count, so we add 1.
fn score_report_due(match_count: u64, scoreinterval: i32) -> bool {
    let idx = match_count + 1;
    idx.is_multiple_of(scoreinterval as u64)
}

/// Check whether all matches have been played.
///
/// C++: `match_count_ + 1 == final_matchcount_`
fn matches_exhausted(match_count: u64, final_matchcount: u64) -> bool {
    match_count + 1 == final_matchcount
}

thread_local! {
    static THREAD_CPUS: std::cell::OnceCell<Vec<i32>> = const { std::cell::OnceCell::new() };
}

/// Get or initialize CPU cores for the current thread.
///
/// This consumes CPUs from the affinity manager once per thread and reuses them
/// for all games on this thread.
fn get_thread_cpus(affinity_manager: &Arc<AffinityManager>) -> Vec<i32> {
    THREAD_CPUS.with(|cell| {
        cell.get_or_init(|| {
            match affinity_manager.consume() {
                Ok(guard) => {
                    let cpus = guard.cpus().to_vec();

                    // Set thread affinity BEFORE starting engines
                    // This ensures spawned engine processes inherit correct affinity on Linux
                    if !cpus.is_empty() {
                        let tid = crate::affinity::get_thread_handle();
                        let success = crate::affinity::set_thread_affinity(&cpus, tid);
                        if !success {
                            log::warn!("Failed to set CPU affinity for tournament thread");
                        }
                    }

                    // Leak the guard to keep CPUs reserved for the lifetime of this thread
                    // This is safe because the thread-local will live for the entire thread lifetime
                    std::mem::forget(guard);
                    cpus
                }
                Err(e) => {
                    log::warn!("Failed to consume CPU cores: {}", e);
                    Vec::new()
                }
            }
        })
        .clone()
    })
}

/// Unified structure for game player assignment.
///
/// Tracks both the color assignment (who plays white/black) and the original
/// player order (first/second from the pairing) to eliminate confusion between
/// color-based and player-based naming throughout the codebase.
pub struct GameAssignment {
    /// Engine playing white (may be first or second player depending on swaps)
    first: &'static crate::types::engine_config::EngineConfiguration,
    /// Engine playing black (may be first or second player depending on swaps)
    second: &'static crate::types::engine_config::EngineConfiguration,
}

impl GameAssignment {
    pub fn new(
        first: &'static crate::types::engine_config::EngineConfiguration,
        second: &'static crate::types::engine_config::EngineConfiguration,
    ) -> Self {
        Self { first, second }
    }

    /// Get the first player (original pairing order, for stats).
    pub fn first(&self) -> &'static crate::types::engine_config::EngineConfiguration {
        self.first
    }

    /// Get the second player (original pairing order, for stats).
    pub fn second(&self) -> &'static crate::types::engine_config::EngineConfiguration {
        self.second
    }

    /// Get the first player's name.
    pub fn first_name(&self) -> &'static str {
        self.first().name.as_str()
    }

    /// Get the second player's name.
    pub fn second_name(&self) -> &'static str {
        self.second().name.as_str()
    }
}

/// Prepare engine configs with correct colors for this game.
///
/// Handles color swapping for even games and reverse option.
fn prepare_engine_configs(pairing: &Pairing, noswap: bool, reverse: bool) -> GameAssignment {
    let engine_configs = config::engine_configs();
    let first = &engine_configs[pairing.player1];
    let second = &engine_configs[pairing.player2];

    let (first, second) = if pairing.game_id.is_multiple_of(2) && !noswap {
        // Swap colors for even games
        (second, first)
    } else {
        (first, second)
    };

    // // Apply reverse option (swaps colors again)
    if reverse {
        panic!("Reverse option is not supported in this implementation");
    }
    let (first, second) = if reverse {
        (second, first)
    } else {
        (first, second)
    };

    GameAssignment { first, second }
}

/// Restart an engine if it's unresponsive.
fn restart_engine_if_unresponsive(engine: &EngineGuard, engine_name: &str) {
    let mut engine = engine.lock();

    if engine.isready(None).is_ok() {
        return;
    }

    log::trace!("Restarting engine {}", engine_name);
    if let Err(e) = engine.restart() {
        crate::set_abnormal_termination();
        crate::set_stop();
        log::error!("Failed to restart engine: {}", e);
    }
}

/// Run a single match between two engines.
///
/// Returns the match result and engine info, or None if the game should stop.
///
/// Note: We pass first/second players (from the pairing) to Match::start.
/// The Match determines who actually plays white/black based on the opening.
fn run_match(
    assignment: &GameAssignment,
    opening: crate::game::book::Opening,
    cpus_for_game: Option<&[i32]>,
    state: &SharedState,
    pairing: &Pairing,
) -> Result<(Match, EngineDisplayInfo, EngineDisplayInfo), RunGameError> {
    let tournament_config = config::tournament_config();

    // Get engines by their player order (first/second), not color
    let first_guard = match state.engine_cache.get_engine(
        &assignment.first().name,
        assignment.first(),
        tournament_config.log.realtime,
    ) {
        Ok(guard) => guard,
        Err(e) => {
            return Err(RunGameError::FirstEngineCreation(e.to_string()));
        }
    };

    let second_guard = match state.engine_cache.get_engine(
        &assignment.second().name,
        assignment.second(),
        tournament_config.log.realtime,
    ) {
        Ok(guard) => guard,
        Err(e) => {
            return Err(RunGameError::SecondEngineCreation(e.to_string()));
        }
    };

    let (game, first_info, second_info) = {
        let mut first_ref = first_guard.lock();
        let mut second_ref = second_guard.lock();

        // Run the match — lock both engines for the duration of the game
        // Match::start determines who plays white based on the opening
        let mut game = Match::new(opening, tournament_config);
        game.start(&mut first_ref, &mut second_ref, cpus_for_game);

        // Get the actual white/black info from the match result
        let match_data = game.get();
        let white_name = &match_data.players.white.config.name;
        let black_name = &match_data.players.black.config.name;

        log::trace!(
            "Game {} between {} (white) and {} (black) finished",
            pairing.game_id,
            white_name,
            black_name
        );

        // Determine which engine reference is white/black for display info
        // let (white_ref, black_ref) = if *white_name == assignment.first().name {
        //     (&first_ref, &second_ref)
        // } else {
        //     (&second_ref, &first_ref)
        // };

        let first_info = EngineDisplayInfo::from_uci_engine(&first_ref);
        let second_info = EngineDisplayInfo::from_uci_engine(&second_ref);

        // Handle stall/disconnect
        if game.is_stall_or_disconnect() {
            if !tournament_config.recover {
                return Err(RunGameError::GameStalled(pairing.game_id));
            }

            restart_engine_if_unresponsive(&first_guard, &assignment.first().name);
            restart_engine_if_unresponsive(&second_guard, &assignment.second().name);
        }

        (game, first_info, second_info)
    };

    Ok((game, first_info, second_info))
}

/// Convert a game result to stats.
fn result_to_stats(result: GameResult) -> Stats {
    match result {
        GameResult::Win => Stats::new(1, 0, 0),
        GameResult::Lose => Stats::new(0, 1, 0),
        GameResult::Draw => Stats::new(0, 0, 1),
        GameResult::None => Stats::new(0, 0, 0),
    }
}

/// Check SPRT and return whether the tournament should stop.
fn check_sprt(
    state: &SharedState,
    stats: &Stats,
    assignment: &GameAssignment,
    white_info: &EngineDisplayInfo,
    black_info: &EngineDisplayInfo,
    pair_done: bool,
    is_last: bool,
    rating_due: bool,
) -> bool {
    if !state.sprt.is_enabled() {
        return false;
    }

    let tournament_config = config::tournament_config();
    let llr = state.sprt.get_llr(stats, tournament_config.report_penta);
    let sprt_result = state.sprt.get_result(llr);

    if sprt_result == SprtResult::Continue && !is_last {
        return false;
    }

    log::trace!("SPRT test finished, stopping tournament.");
    crate::set_stop();

    let termination_message = format!(
        "SPRT ({}) completed - {} was accepted",
        state.sprt.get_elo(),
        if sprt_result == SprtResult::H0 {
            "H0"
        } else {
            "H1"
        }
    );

    log::info!("{}", termination_message);

    let out = state.output.lock().unwrap();
    // Only print these if we haven't already printed them above
    if !(rating_due && pair_done) && !is_last {
        out.print_result(stats, assignment.first_name(), assignment.second_name());
        out.print_interval(
            &state.sprt,
            stats,
            assignment.first_name(),
            assignment.second_name(),
            (white_info, black_info),
            &state.opening_file,
            &state.scoreboard,
        );
    }
    out.end_tournament(&termination_message);

    true
}

/// Write match results to output files.
fn write_match_files(state: &SharedState, match_data: &MatchData, round_id: usize) {
    // Write PGN file
    if let Some(ref writer) = state.file_writer_pgn {
        let tournament_config = config::tournament_config();
        let pgn = PgnBuilder::build(&tournament_config.pgn, match_data, round_id);

        match pgn {
            Ok(content) => writer.write(&content),
            Err(e) => log::warn!("Failed to build PGN: {}", e),
        }
    }

    // Write EPD file (final position FEN)
    if let Some(ref writer) = state.file_writer_epd {
        if !match_data.fen.is_empty() {
            writer.write(&format!("{}\n", match_data.fen));
        }
    }
}

/// Schedule the next game if not stopping.
fn schedule_next_game(state: &SharedState) {
    if crate::is_stop() {
        return;
    }

    let next_pairing = {
        let mut sched = state.scheduler.lock().unwrap();
        sched.as_mut().and_then(|s| s.next())
    };

    if let Some(next) = next_pairing {
        let next_state = state.clone();
        rayon::spawn(move || {
            run_game(next, next_state);
        });
    }
}

/// Track timeouts and disconnects for a player.
fn track_timeouts(state: &SharedState, match_data: &MatchData) {
    if let Some(loser) = match_data.get_losing_player() {
        let t = state.tracker.lock().unwrap();
        match match_data.termination {
            MatchTermination::Timeout => {
                t.report_timeout(&loser.config.name);
            }
            MatchTermination::Disconnect | MatchTermination::Stall => {
                t.report_disconnect(&loser.config.name);
            }
            _ => {}
        }
    }
}

/// Run a single game and schedule the next one when done.
///
/// This is a free function (not a method) so it can be called from within
/// worker closures without borrowing `Tournament`.
fn run_game(pairing: Pairing, state: SharedState) {
    if crate::is_stop() {
        return;
    }

    let tournament_config = config::tournament_config();

    // Get CPU affinity for this thread (if enabled)
    let cpus_for_game = tournament_config
        .affinity
        .then(|| get_thread_cpus(&state.affinity_manager));

    // Prepare engine configs with correct colors
    let assignment = prepare_engine_configs(
        &pairing,
        tournament_config.noswap,
        tournament_config.reverse,
    );

    // Get opening from the scheduler's book
    let opening = {
        let sched = state.scheduler.lock().unwrap();
        sched
            .as_ref()
            .map(|s| s.get_opening(pairing.opening_id))
            .unwrap_or_default()
    };

    log::trace!(
        "Game {} between {} and {} starting",
        pairing.game_id,
        assignment.first().name,
        assignment.second().name
    );

    // Print start
    {
        let out = state.output.lock().unwrap();
        out.start_game(
            &assignment,
            pairing.game_id,
            state.final_matchcount as usize,
        );
    }

    // Run the match
    let cpus_slice = cpus_for_game.as_deref();
    let (game, first_info, second_info) =
        match run_match(&assignment, opening, cpus_slice, &state, &pairing) {
            Ok(result) => result,
            Err(RunGameError::GameStalled(id)) => {
                if !crate::STOP.swap(true, Ordering::Relaxed) {
                    log::warn!(
                        "Game {} stalled/disconnected, no recover option set, stopping tournament.",
                        id
                    );
                    crate::set_abnormal_termination();
                }
                return;
            }
            Err(e) => {
                log::error!("{}", e);
                crate::set_abnormal_termination();
                crate::set_stop();
                return;
            }
        };

    let match_data = game.get();

    // Record results if the game completed normally
    if match_data.termination == MatchTermination::Interrupt || crate::is_stop() {
        track_timeouts(&state, &match_data);
        return;
    }

    // Build stats from first perspective, this changes according to pairing
    let Some(first_player_info) = match_data.players.get_first_moved() else {
        log::error!("No first move player found, internal error");
        crate::set_abnormal_termination();
        crate::set_stop();
        return;
    };

    let stats = result_to_stats(first_player_info.result);

    // Process results and output
    process_game_results(
        &state,
        &assignment,
        &stats,
        &match_data,
        &first_info,
        &second_info,
        &pairing,
    );

    // Track timeouts/disconnects
    track_timeouts(&state, &match_data);
}

/// Reporting mode abstraction to handle penta vs standard mode differences.
enum ReportingMode {
    Penta { pairing_id: u64 },
    Standard,
}

impl ReportingMode {
    fn from_config(report_penta: bool, pairing_id: usize) -> Self {
        if report_penta {
            Self::Penta {
                pairing_id: pairing_id as u64,
            }
        } else {
            Self::Standard
        }
    }

    fn update_scoreboard(&self, scoreboard: &ScoreBoard, configs: &GameAssignment, stats: &Stats) {
        match self {
            Self::Penta { pairing_id } => {
                scoreboard.update_pair(configs, stats, *pairing_id);
            }
            Self::Standard => {
                scoreboard.update_non_pair(configs, stats);
            }
        }
    }

    fn rating_interval_index(&self, match_count: u64) -> u64 {
        match self {
            Self::Penta { pairing_id } => *pairing_id + 1,
            Self::Standard => match_count + 1,
        }
    }

    fn rating_report_due(&self, match_count: u64, ratinginterval: i32) -> bool {
        let idx = self.rating_interval_index(match_count);
        idx % ratinginterval as u64 == 0
    }

    fn pair_finished(&self, scoreboard: &ScoreBoard) -> bool {
        match self {
            Self::Penta { pairing_id } => scoreboard.is_pair_completed(*pairing_id),
            Self::Standard => true,
        }
    }
}

/// Process game results: update scoreboard, print output, check SPRT, write files.
fn process_game_results(
    state: &SharedState,
    assignment: &GameAssignment,
    stats: &Stats,
    match_data: &MatchData,
    first_info: &EngineDisplayInfo,
    second_info: &EngineDisplayInfo,
    pairing: &Pairing,
) {
    let config = config::tournament_config();
    let reporting_mode = ReportingMode::from_config(config.report_penta, pairing.pairing_id);

    // Lock the output mutex to serialize the entire end-game output block.
    // This prevents interleaved output from concurrent workers.
    let _output_guard = state.output_mutex.lock().unwrap();

    {
        let out = state.output.lock().unwrap();
        out.end_game(&assignment, stats, &match_data.reason, pairing.game_id);
    }

    // ScoreBoard has interior mutability, no need for outer lock
    reporting_mode.update_scoreboard(&state.scoreboard, assignment, stats);

    let current_count = state.match_count.load(Ordering::Relaxed);
    let is_last = matches_exhausted(current_count, state.final_matchcount);

    let stats = state
        .scoreboard
        .get_stats(assignment.first_name(), assignment.second_name());

    // Print score interval (e.g., "Score of X vs Y: W - L - D [ratio] N")
    if score_report_due(current_count, config.scoreinterval) || is_last {
        let out = state.output.lock().unwrap();
        out.print_result(&stats, assignment.first_name(), assignment.second_name());
    }

    let rating_due = reporting_mode.rating_report_due(current_count, config.ratinginterval);
    let pair_done = reporting_mode.pair_finished(&state.scoreboard);

    if (rating_due && pair_done) || is_last {
        let out = state.output.lock().unwrap();
        out.print_interval(
            &state.sprt,
            &stats,
            assignment.first_name(),
            assignment.second_name(),
            (first_info, second_info),
            &state.opening_file,
            &state.scoreboard,
        );
    }

    // Check SPRT
    let sprt_stopped = check_sprt(
        state,
        &stats,
        assignment,
        first_info,
        second_info,
        pair_done,
        is_last,
        rating_due,
    );

    drop(_output_guard);

    // Write PGN and EPD files
    write_match_files(state, match_data, pairing.round_id);

    // Increment match count
    state.match_count.fetch_add(1, Ordering::Relaxed);

    // Schedule next game (only if SPRT didn't stop us and we're not stopping)
    if !sprt_stopped {
        schedule_next_game(state);
    }
}
