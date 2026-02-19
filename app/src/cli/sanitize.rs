//! Configuration validation and sanitization.
//!
//! Ports `cli/sanitize.cpp` from C++.

use crate::types::engine_config::EngineConfiguration;
use crate::types::enums::*;
use crate::types::tournament::TournamentConfig;
use crate::{log_info, log_warn};
#[cfg(target_os = "windows")]
use std::ffi::OsStr;
#[cfg(target_os = "windows")]
use std::path::PathBuf;

/// Sanitize and validate the tournament configuration.
pub fn sanitize_tournament(config: &mut TournamentConfig) -> Result<(), String> {
    fix_config(config)?;
    set_defaults(config);
    adjust_concurrency(config)?;
    validate_config(config)?;
    Ok(())
}

/// Sanitize and validate engine configurations.
pub fn sanitize_engines(configs: &mut [EngineConfiguration]) -> Result<(), String> {
    if configs.len() < 2 {
        return Err("Error: Need at least two engines to start!".to_string());
    }

    for (i, config) in configs.iter().enumerate() {
        validate_engine(config)?;

        for previous_config in &configs[..i] {
            if config.name == previous_config.name {
                return Err(format!(
                    "Error: Engines with the same name are not allowed!: {}",
                    config.name
                ));
            }
        }
    }

    #[cfg(target_os = "windows")]
    for config in configs.iter_mut() {
        // Ensure .exe extension on Windows if not already present
        config.cmd = ensure_exe_extension(&config.cmd);
    }

    Ok(())
}

/// Fix obviously wrong config values (swap games/rounds, fix penta).
fn fix_config(config: &mut TournamentConfig) -> Result<(), String> {
    if config.games > 2 {
        // Likely games and rounds are swapped
        std::mem::swap(&mut config.games, &mut config.rounds);

        if config.games > 2 {
            return Err("Error: Exceeded -game limit! Must be less than 2".to_string());
        }
    }

    // Pentanomial reporting only makes sense with fastchess output and games=2
    if config.report_penta && config.output == OutputType::Cutechess {
        config.report_penta = false;
    }
    if config.report_penta && config.games != 2 {
        config.report_penta = false;
    }

    Ok(())
}

/// Set sensible defaults for unset values.
fn set_defaults(config: &mut TournamentConfig) {
    if config.ratinginterval == 0 {
        config.ratinginterval = i32::MAX;
    }
    if config.scoreinterval == 0 {
        config.scoreinterval = i32::MAX;
    }
}

/// Adjust and validate concurrency setting.
fn adjust_concurrency(config: &mut TournamentConfig) -> Result<(), String> {
    let hw_threads = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(1);

    // Handle the special case of concurrency == 0 (e.g., from config files).
    // Negative values are handled during CLI parsing in mod.rs.
    if config.concurrency == 0 {
        config.concurrency = hw_threads;
        log_info!(
            "Info: Adjusted concurrency to {} based on number of available hardware threads.",
            config.concurrency
        );
    }

    if config.concurrency > hw_threads && !config.force_concurrency {
        return Err(
            "Error: Concurrency exceeds number of CPUs. Use -force-concurrency to override."
                .to_string(),
        );
    }

    // Check file descriptor limits on Linux
    #[cfg(target_os = "linux")]
    {
        // Each concurrent game needs approximately 6 FDs (2 engines x 3 pipes)
        // plus some overhead for logging, PGN files, etc.
        let fds_per_game: usize = 8;
        let min_fds = config.concurrency * fds_per_game + 64; // 64 for overhead

        if let Ok(rlim) = nix::sys::resource::getrlimit(nix::sys::resource::Resource::RLIMIT_NOFILE)
        {
            let max_fds = rlim.0 as usize; // soft limit
            if max_fds < min_fds {
                log_warn!(
                    "There aren't enough file descriptors available for the specified concurrency.\n\
                     Please increase the limit using ulimit -n 65536.\n\
                     The maximum number of file descriptors required for this configuration is: {}",
                    min_fds
                );

                let max_concurrency = (max_fds.saturating_sub(64)) / fds_per_game;
                if max_concurrency < 1 {
                    return Err(
                        "Error: Not enough file descriptors available for the specified concurrency."
                            .to_string(),
                    );
                }

                log_warn!("Limiting concurrency to: {}", max_concurrency);
                config.concurrency = max_concurrency;
            }
        }
    }

    Ok(())
}

/// Validate tournament configuration settings.
fn validate_config(config: &mut TournamentConfig) -> Result<(), String> {
    // Validate SPRT config
    if config.sprt.enabled {
        crate::matchmaking::sprt::Sprt::validate(
            config.sprt.alpha,
            config.sprt.beta,
            config.sprt.elo0,
            config.sprt.elo1,
            &config.sprt.model,
            &mut config.report_penta,
        )?;
    }

    // FRC requires an opening book
    if config.variant == VariantType::Frc && config.opening.file.is_empty() {
        return Err("Error: Please specify a Chess960 opening book".to_string());
    }

    // Warn about missing opening book
    if config.opening.file.is_empty() {
        log_warn!(
            "Warning: No opening book specified! Consider using one, otherwise all games \
             will be played from the starting position."
        );
    }

    // Warn about unknown opening format
    if config.opening.format != FormatType::Epd && config.opening.format != FormatType::Pgn {
        log_warn!(
            "Warning: Unknown opening format. All games will be played from the starting position."
        );
    }

    // Validate tablebase adjudication
    if config.tb_adjudication.enabled && config.tb_adjudication.syzygy_dirs.is_empty() {
        return Err(
            "Error: Must provide a ;-separated list of Syzygy tablebase directories.".to_string(),
        );
    }

    Ok(())
}

#[cfg(target_os = "windows")]
fn ensure_exe_extension(input: &str) -> String {
    let mut path = PathBuf::from(input);

    if path.extension().and_then(OsStr::to_str) != Some("exe") {
        path.set_extension("exe");
    }

    path.to_string_lossy().into_owned()
}

/// Validate a single engine configuration.
fn validate_engine(config: &EngineConfiguration) -> Result<(), String> {
    if config.name.is_empty() {
        return Err("Error: Please specify a name for each engine!".to_string());
    }

    let has_tc =
        (config.limit.tc.time + config.limit.tc.increment) != 0 || config.limit.tc.fixed_time != 0;
    let has_nodes = config.limit.nodes != 0;
    let has_plies = config.limit.plies != 0;

    if !has_tc && !has_nodes && !has_plies {
        return Err("Error: No TimeControl specified!".to_string());
    }

    let tc_count = (((config.limit.tc.time + config.limit.tc.increment) != 0) as u32)
        + ((config.limit.tc.fixed_time != 0) as u32);
    if tc_count > 1 {
        return Err("Error: Cannot use tc and st together!".to_string());
    }

    // Check engine binary exists (if dir is specified or path is absolute)
    let engine_path = if config.dir.is_empty() {
        std::path::PathBuf::from(&config.cmd)
    } else {
        std::path::Path::new(&config.dir).join(&config.cmd)
    };

    if (!config.dir.is_empty() || engine_path.is_absolute()) && !engine_path.is_file() {
        return Err(format!(
            "Engine binary does not exist: {}",
            engine_path.display()
        ));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fix_config_swap() {
        let mut config = TournamentConfig::default();
        config.games = 100; // clearly wrong — should be rounds
        config.rounds = 2;
        fix_config(&mut config).unwrap();
        assert_eq!(config.games, 2);
        assert_eq!(config.rounds, 100);
    }

    #[test]
    fn test_fix_config_penta_cutechess() {
        let mut config = TournamentConfig::default();
        config.report_penta = true;
        config.output = OutputType::Cutechess;
        config.games = 2;
        fix_config(&mut config).unwrap();
        assert!(!config.report_penta);
    }

    #[test]
    fn test_validate_engine_no_name() {
        let config = EngineConfiguration::default();
        assert!(validate_engine(&config).is_err());
    }

    #[test]
    fn test_validate_engine_no_tc() {
        let mut config = EngineConfiguration::default();
        config.name = "test".to_string();
        config.cmd = "test".to_string();
        assert!(validate_engine(&config).is_err());
    }

    #[test]
    fn test_validate_engine_ok() {
        let mut config = EngineConfiguration::default();
        config.name = "test".to_string();
        config.cmd = "test".to_string();
        config.limit.tc.time = 10000;
        config.limit.tc.increment = 100;
        // Don't check file existence when dir is empty and path is relative
        assert!(validate_engine(&config).is_ok());
    }

    #[test]
    fn test_sanitize_engines_too_few() {
        let mut configs = vec![EngineConfiguration::default()];
        assert!(sanitize_engines(&mut configs).is_err());
    }

    #[test]
    fn test_sanitize_engines_duplicate_names() {
        let mut e1 = EngineConfiguration::default();
        e1.name = "SF".to_string();
        e1.cmd = "stockfish".to_string();
        e1.limit.tc.time = 10000;

        let e2 = e1.clone();
        let mut configs = vec![e1, e2];
        assert!(sanitize_engines(&mut configs).is_err());
    }

    #[test]
    fn test_set_defaults() {
        let mut config = TournamentConfig::default();
        config.ratinginterval = 0;
        config.scoreinterval = 0;
        set_defaults(&mut config);
        assert_eq!(config.ratinginterval, i32::MAX);
        assert_eq!(config.scoreinterval, i32::MAX);
    }
}
