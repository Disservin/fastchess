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

use crate::affinity::AffinityManager;
use crate::core::config;
use crate::core::file_writer::FileWriter;
use crate::core::threadpool::{TaskSender, ThreadPool};
use crate::engine::cache::EngineCache;
use crate::game::book::OpeningBook;
use crate::game::pgn::PgnBuilder;
use crate::matchmaking::match_runner::Match;
use crate::matchmaking::output::create_output;
use crate::matchmaking::scoreboard::{PlayerPairKey, ScoreBoard, StatsMap};
use crate::matchmaking::sprt::{Sprt, SprtModel, SprtResult};
use crate::matchmaking::stats::Stats;
use crate::matchmaking::timeout_tracker::PlayerTracker;
use crate::matchmaking::tournament::schedule::{Pairing, Scheduler, SchedulerVariant};
use crate::types::engine_config::GamePair;
use crate::types::match_data::*;

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
    sender: TaskSender,
    file_writer_pgn: Option<Arc<Mutex<FileWriter>>>,
    file_writer_epd: Option<Arc<Mutex<FileWriter>>>,
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
    pool: ThreadPool,
    output: Arc<Mutex<Box<dyn crate::matchmaking::output::Output + Send>>>,
    output_mutex: Arc<Mutex<()>>,
    sprt: Arc<Sprt>,

    match_count: Arc<std::sync::atomic::AtomicU64>,
    initial_matchcount: u64,
    final_matchcount: u64,

    file_writer_pgn: Option<Arc<Mutex<FileWriter>>>,
    file_writer_epd: Option<Arc<Mutex<FileWriter>>>,

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
            Some(Arc::new(Mutex::new(
                FileWriter::new(
                    &tournament_config.pgn.file,
                    append,
                    tournament_config.pgn.crc,
                )
                .map_err(|e| e.to_string())?,
            )))
        } else {
            None
        };

        let file_writer_epd = if !tournament_config.epd.file.is_empty() {
            let append = tournament_config.epd.append_file || initial_matchcount > 0;
            Some(Arc::new(Mutex::new(
                FileWriter::new(&tournament_config.epd.file, append, false)
                    .map_err(|e| e.to_string())?,
            )))
        } else {
            None
        };

        let pool = ThreadPool::new(tournament_config.concurrency);

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
            pool,
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
        while self.match_count.load(Ordering::Relaxed) < self.final_matchcount
            && !crate::STOP.load(Ordering::Relaxed)
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
            sender: self.pool.sender(),
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

        let num_threads = self.pool.num_threads();
        let state = self.shared_state();

        for _ in 0..num_threads {
            let pairing = {
                let mut sched = state.scheduler.lock().unwrap();
                sched.as_mut().and_then(|s| s.next())
            };

            if let Some(pairing) = pairing {
                let state = state.clone();
                self.pool.enqueue(move || {
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
        let mut seen = Vec::new();

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
                seen.push(pair);
            }
        }

        map
    }
}

impl Drop for Tournament {
    fn drop(&mut self) {
        log::trace!("~Tournament()");
        self.pool.kill();
    }
}

/// Check whether the score interval should trigger at the current match count.
///
/// C++: `(match_count_ + 1) % cfg.scoreinterval == 0`
/// Called before incrementing match_count, so we add 1.
fn should_print_score_interval(match_count: u64, scoreinterval: i32) -> bool {
    let idx = match_count + 1;
    idx.is_multiple_of(scoreinterval as u64)
}

/// Check whether the rating interval should trigger.
///
/// C++: When `report_penta`, uses `pairing_id + 1`; otherwise `match_count_ + 1`.
fn should_print_rating_interval(
    match_count: u64,
    pairing_id: usize,
    report_penta: bool,
    ratinginterval: i32,
) -> bool {
    let idx = if report_penta {
        pairing_id as u64 + 1
    } else {
        match_count + 1
    };
    idx % ratinginterval as u64 == 0
}

/// Check whether all matches have been played.
///
/// C++: `match_count_ + 1 == final_matchcount_`
fn all_matches_played(match_count: u64, final_matchcount: u64) -> bool {
    match_count + 1 == final_matchcount
}

/// Run a single game and schedule the next one when done.
///
/// This is a free function (not a method) so it can be called from within
/// worker closures without borrowing `Tournament`.
fn run_game(pairing: Pairing, state: SharedState) {
    if crate::STOP.load(Ordering::Relaxed) {
        return;
    }

    let engine_configs = config::engine_configs();
    let tournament_config = config::tournament_config();
    let rl = tournament_config.log.realtime;

    // Thread-local CPU affinity (consume once per thread, reuse for all games on this thread)
    thread_local! {
        static THREAD_CPUS: std::cell::OnceCell<Vec<i32>> = const { std::cell::OnceCell::new() };
    }

    // Consume CPUs for this thread if affinity is enabled
    let cpus_for_game: Option<Vec<i32>> = if tournament_config.affinity {
        Some(THREAD_CPUS.with(|cell| {
            cell.get_or_init(|| {
                match state.affinity_manager.consume() {
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

                        // Leak the guard so CPUs stay reserved for the lifetime of this thread
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
        }))
    } else {
        None
    };

    let first = &engine_configs[pairing.player1];
    let second = &engine_configs[pairing.player2];

    // Store names before moving configs - these are used later for stats lookup
    // regardless of which color each player ends up playing
    let first_name = first.name.clone();
    let second_name = second.name.clone();

    // Move the cloned configs into GamePair instead of cloning again
    let mut configs = GamePair::new(first, second);

    // Swap colors for even games (unless noswap is set)
    if pairing.game_id.is_multiple_of(2) && !tournament_config.noswap {
        std::mem::swap(&mut configs.white, &mut configs.black);
    }

    if tournament_config.reverse {
        std::mem::swap(&mut configs.white, &mut configs.black);
    }

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
        configs.white.name,
        configs.black.name
    );

    // Print start
    {
        let out = state.output.lock().unwrap();
        out.start_game(&configs, pairing.game_id, state.final_matchcount as usize);
    }

    // Check out engines from the cache
    let white_guard = state
        .engine_cache
        .get_engine(&configs.white.name, &configs.white, rl);
    let black_guard = state
        .engine_cache
        .get_engine(&configs.black.name, &configs.black, rl);

    // Run the match — lock both engines for the duration of the game
    let mut game = Match::new(opening, tournament_config);
    {
        let mut white_ref = white_guard.lock();
        let mut black_ref = black_guard.lock();
        // Pass CPUs to engines (will be set on engine processes)
        let cpus_slice = cpus_for_game.as_deref();
        game.start(&mut white_ref, &mut black_ref, cpus_slice);
    }

    let match_data = game.get().clone();

    log::trace!(
        "Game {} between {} and {} finished",
        pairing.game_id,
        configs.white.name,
        configs.black.name
    );

    // Handle stall/disconnect
    if game.is_stall_or_disconnect() {
        if !tournament_config.recover {
            if !crate::STOP.swap(true, Ordering::Relaxed) {
                log::warn!(
                    "Game {} stalled/disconnected, no recover option set, stopping tournament.",
                    pairing.game_id
                );
                crate::ABNORMAL_TERMINATION.store(true, Ordering::Relaxed);
            }
            return;
        }

        // Restart engines that are not responsive (recover mode)
        {
            let mut white_ref = white_guard.lock();
            let responsive = white_ref.isready(None).code == crate::engine::process::Status::Ok;
            drop(white_ref);
            if !responsive {
                log::trace!("Restarting white engine {}", configs.white.name);
                white_guard.restart_engine();
            }
        }
        {
            let mut black_ref = black_guard.lock();
            let responsive = black_ref.isready(None).code == crate::engine::process::Status::Ok;
            drop(black_ref);
            if !responsive {
                log::trace!("Restarting black engine {}", configs.black.name);
                black_guard.restart_engine();
            }
        }
    }

    // Record results if the game completed normally
    if match_data.termination != MatchTermination::Interrupt && !crate::STOP.load(Ordering::Relaxed)
    {
        // Build stats from white's perspective
        let stats = match match_data.players.white.result {
            GameResult::Win => Stats::new(1, 0, 0),
            GameResult::Lose => Stats::new(0, 1, 0),
            GameResult::Draw => Stats::new(0, 0, 1),
            GameResult::None => Stats::new(0, 0, 0),
        };

        // Lock the output mutex to serialize the entire end-game output block.
        // This prevents interleaved output from concurrent workers.
        let _output_guard = state.output_mutex.lock().unwrap();

        {
            let out = state.output.lock().unwrap();
            out.end_game(&configs, &stats, &match_data.reason, pairing.game_id);
        }

        // ScoreBoard has interior mutability, no need for outer lock
        if tournament_config.report_penta {
            state
                .scoreboard
                .update_pair(&configs, &stats, pairing.pairing_id as u64);
        } else {
            state.scoreboard.update_non_pair(&configs, &stats);
        }

        let current_count = state.match_count.load(Ordering::Relaxed);
        let is_last = all_matches_played(current_count, state.final_matchcount);

        // Get updated stats for the pair
        let updated_stats = state.scoreboard.get_stats(&first_name, &second_name);

        // Print score interval (e.g., "Score of X vs Y: W - L - D [ratio] N")
        if should_print_score_interval(current_count, tournament_config.scoreinterval) || is_last {
            let out = state.output.lock().unwrap();
            out.print_result(&updated_stats, &first_name, &second_name);
        }

        // Print rating interval (Elo, SPRT, etc.)
        let should_print_rating = should_print_rating_interval(
            current_count,
            pairing.pairing_id,
            tournament_config.report_penta,
            tournament_config.ratinginterval,
        );

        let pair_completed = if tournament_config.report_penta {
            state
                .scoreboard
                .is_pair_completed(pairing.pairing_id as u64)
        } else {
            true
        };

        if (should_print_rating && pair_completed) || is_last {
            let white_ref = white_guard.lock();
            let black_ref = black_guard.lock();
            let out = state.output.lock().unwrap();
            out.print_interval(
                &state.sprt,
                &updated_stats,
                &first_name,
                &second_name,
                (&*white_ref, &*black_ref),
                &state.opening_file,
                &state.scoreboard,
            );
        }

        // Check SPRT
        if state.sprt.is_enabled() {
            let llr = state
                .sprt
                .get_llr(&updated_stats, tournament_config.report_penta);
            let sprt_result = state.sprt.get_result(llr);

            if sprt_result != SprtResult::Continue || is_last {
                log::trace!("SPRT test finished, stopping tournament.");
                crate::STOP.store(true, Ordering::Relaxed);

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
                if !((should_print_rating && pair_completed) || is_last) {
                    out.print_result(&updated_stats, &first_name, &second_name);
                    let white_ref = white_guard.lock();
                    let black_ref = black_guard.lock();
                    out.print_interval(
                        &state.sprt,
                        &updated_stats,
                        &first_name,
                        &second_name,
                        (&*white_ref, &*black_ref),
                        &state.opening_file,
                        &state.scoreboard,
                    );
                }
                out.end_tournament(&termination_message);
            }
        }

        // Drop the output guard before file I/O
        drop(_output_guard);

        // Write PGN file
        if let Some(ref writer) = state.file_writer_pgn {
            let pgn_str = PgnBuilder::build(&tournament_config.pgn, &match_data, pairing.round_id);
            if let Ok(w) = writer.lock() {
                w.write(&pgn_str);
            }
        }

        // Write EPD file (final position FEN)
        if let Some(ref writer) = state.file_writer_epd {
            if !match_data.fen.is_empty() {
                if let Ok(w) = writer.lock() {
                    w.write(&format!("{}\n", match_data.fen));
                }
            }
        }

        state.match_count.fetch_add(1, Ordering::Relaxed);

        // Schedule next game (only if not stopping)
        if !crate::STOP.load(Ordering::Relaxed) {
            let next_pairing = {
                let mut sched = state.scheduler.lock().unwrap();
                sched.as_mut().and_then(|s| s.next())
            };

            if let Some(next) = next_pairing {
                let next_state = state.clone();
                state.sender.enqueue(move || {
                    run_game(next, next_state);
                });
            }
        }
    }

    // Drop engine guards (returns engines to cache, or deletes if restart=on)
    // This happens automatically, but we do it before timeout tracking for clarity.
    drop(white_guard);
    drop(black_guard);

    // Track timeouts/disconnects
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
