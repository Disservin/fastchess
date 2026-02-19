use std::sync::atomic::Ordering;
use std::time::Instant;

use fastchess::cli::OptionsParser;
use fastchess::core::config;
use fastchess::core::logger::LOGGER;
use fastchess::game::syzygy;
use fastchess::matchmaking::tournament::runner::Tournament;
use fastchess::matchmaking::tournament::schedule::SchedulerVariant;
use fastchess::types::enums::TournamentType;
use fastchess::{log_info, log_trace};

fn run() -> Result<(), String> {
    let start_time = Instant::now();

    // 1. Parse CLI arguments
    let args: Vec<String> = std::env::args().collect();
    let parser = OptionsParser::new(&args)?;
    let (tournament_config, engine_configs, results) = parser.into_parts();

    // 2. Configure logging
    //    Set up our custom logger for engine communication / file logging
    LOGGER.setup(
        &tournament_config.log.file,
        tournament_config.log.level,
        tournament_config.log.append_file,
        tournament_config.log.compress,
        tournament_config.log.engine_coms,
    );

    // 3. Seed RNG
    //    The C++ code calls srand(config.seed). In Rust, rand is not globally seeded
    //    the same way, but the opening book shuffle uses the seed directly.
    log_trace!("Using seed: {}", tournament_config.seed);

    // 4. Determine scheduler variant from tournament type
    let variant = match tournament_config.r#type {
        TournamentType::RoundRobin => SchedulerVariant::RoundRobin,
        TournamentType::Gauntlet => SchedulerVariant::Gauntlet {
            n_seeds: tournament_config.gauntlet_seeds.max(1) as usize,
        },
    };

    // 5. Set global configs (must happen before Tournament::new reads them)
    config::set_tournament_config(tournament_config);
    config::set_engine_configs(engine_configs);

    // 5.5 Initialize Syzygy tablebases if configured
    let tb_config = &config::tournament_config().tb_adjudication;
    if tb_config.enabled && !tb_config.syzygy_dirs.is_empty() {
        syzygy::init_tablebase(&tb_config.syzygy_dirs);
        if syzygy::is_available() {
            log_info!(
                "Syzygy tablebases initialized (up to {} pieces)",
                syzygy::max_pieces()
            );
        }
    }

    // 6. Set up Ctrl+C handler
    ctrlc::set_handler(move || {
        if !fastchess::STOP.swap(true, Ordering::SeqCst) {
            eprintln!("\nInterrupted — stopping tournament...");
        }
    })
    .map_err(|e| format!("Failed to set Ctrl+C handler: {}", e))?;

    // 7. Create and run the tournament
    let mut tournament = Tournament::new(&results, variant)?;

    log_info!("Starting tournament...");
    tournament.start();

    // 8. Report elapsed time
    let elapsed = start_time.elapsed();
    let total_secs = elapsed.as_secs();
    let hours = total_secs / 3600;
    let minutes = (total_secs % 3600) / 60;
    let seconds = total_secs % 60;

    println!("Finished in {}h:{}m:{}s", hours, minutes, seconds);

    // 9. Check for abnormal termination
    if fastchess::ABNORMAL_TERMINATION.load(Ordering::Relaxed) {
        let config_name = &config::tournament_config().config_name;
        eprintln!(
            "The tournament was interrupted abnormally.\n\
             To resume, use: -config file={}",
            config_name
        );
        // Finalize the logger before returning error
        LOGGER.finish();
        return Err("Abnormal termination".to_string());
    }

    // Finalize the logger to ensure gzip trailer is written
    LOGGER.finish();

    Ok(())
}

fn main() {
    let exit_code = match run() {
        Ok(()) => 0,
        Err(e) => {
            eprintln!("Error: {}", e);
            1
        }
    };

    // Ensure logger is finalized before exit.
    // This is critical for gzip compression to write the trailer.
    // We call finish() here again (even though run() calls it) because
    // std::process::exit() bypasses normal cleanup and Drop implementations.
    LOGGER.finish();

    std::process::exit(exit_code);
}
