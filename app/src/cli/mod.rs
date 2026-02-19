//! Command-line argument parser.
//!
//! Ports `cli/cli.hpp`, `cli/cli.cpp`, and `cli/sanitize.cpp` from C++.
//!
//! The parser uses a hand-rolled option-registry pattern similar to the C++ version.
//! Each CLI flag is registered with a `ParamStyle` that determines how its
//! parameters are consumed, and handlers populate an `ArgumentData` struct
//! that contains the `TournamentConfig`, engine configs, and saved stats.

mod sanitize;

use std::collections::HashMap;
use std::time::Duration;

use crate::game::timecontrol::TimeControlLimits;
use crate::matchmaking::scoreboard::StatsMap;
use crate::types::adjudication::TbResultType;
use crate::types::engine_config::EngineConfiguration;
use crate::types::enums::*;
use crate::types::tournament::{LogLevel, TournamentConfig};
use crate::{log_info, log_warn};

// ---------------------------------------------------------------------------
// ArgumentData — the parsing context
// ---------------------------------------------------------------------------

/// Holds all data collected during CLI parsing.
#[derive(Default)]
pub struct ArgumentData {
    pub tournament_config: TournamentConfig,
    pub old_tournament_config: TournamentConfig,
    pub stats: StatsMap,
    pub configs: Vec<EngineConfiguration>,
    pub old_configs: Vec<EngineConfiguration>,
}

// ---------------------------------------------------------------------------
// ParamStyle — how an option consumes its following arguments
// ---------------------------------------------------------------------------

/// Determines how many and what shape of parameters an option expects.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
enum ParamStyle {
    /// Arbitrary list of plain strings.
    Free,
    /// No parameters allowed.
    None,
    /// Exactly one parameter.
    Single,
    /// One or more `key=value` pairs.
    KeyValue,
}

// ---------------------------------------------------------------------------
// Option handler types
// ---------------------------------------------------------------------------

type HandlerFn = Box<dyn Fn(&[String], &mut ArgumentData) -> Result<(), String>>;

struct OptionEntry {
    handler: HandlerFn,
    deferred: bool,
}

// ---------------------------------------------------------------------------
// OptionsParser
// ---------------------------------------------------------------------------

/// The main CLI options parser.
///
/// Parses command-line arguments, populates `TournamentConfig` and engine
/// configurations, applies sanitization/validation, and returns the results.
pub struct OptionsParser {
    data: ArgumentData,
}

impl OptionsParser {
    /// Parse command-line arguments and return a configured parser.
    ///
    /// The `args` should be the raw argv (including program name at index 0).
    pub fn new(args: &[String]) -> Result<Self, String> {
        if args.len() <= 1 {
            // In C++ this prints help and exits; here we return an error.
            return Err("No arguments provided. Use -help for usage information.".to_string());
        }

        let options = Self::register_options();
        let mut data = ArgumentData::default();

        Self::parse_args(args, &options, &mut data)?;

        // Apply variant to all engine configs
        for config in &mut data.configs {
            config.variant = data.tournament_config.variant;
        }

        sanitize::sanitize_tournament(&mut data.tournament_config)?;
        sanitize::sanitize_engines(&mut data.configs)?;

        Ok(Self { data })
    }

    /// Get the parsed engine configurations.
    pub fn engine_configs(&self) -> &[EngineConfiguration] {
        &self.data.configs
    }

    /// Get the parsed tournament configuration.
    pub fn tournament_config(&self) -> &TournamentConfig {
        &self.data.tournament_config
    }

    /// Get the parsed results (for resumption from JSON config).
    pub fn results(&self) -> &StatsMap {
        &self.data.stats
    }

    /// Consume the parser and return owned data.
    pub fn into_parts(self) -> (TournamentConfig, Vec<EngineConfiguration>, StatsMap) {
        (
            self.data.tournament_config,
            self.data.configs,
            self.data.stats,
        )
    }

    // -----------------------------------------------------------------------
    // Core parsing loop
    // -----------------------------------------------------------------------

    fn parse_args(
        args: &[String],
        options: &HashMap<String, OptionEntry>,
        data: &mut ArgumentData,
    ) -> Result<(), String> {
        let mut deferred: HashMap<String, Vec<String>> = HashMap::new();

        let mut i = 1; // skip program name
        while i < args.len() {
            let arg = &args[i];

            let entry = options
                .get(arg.as_str())
                .ok_or_else(|| format!("Unrecognized option: {} parsing failed.", arg))?;

            // Collect parameters until the next flag
            let mut params = Vec::new();
            while i + 1 < args.len() {
                let val = &args[i + 1];
                if val.starts_with('-') {
                    // Allow negative numbers as values
                    if val.len() > 1 && val.as_bytes()[1].is_ascii_digit() {
                        // It's a negative number, consume it
                    } else {
                        break;
                    }
                }
                i += 1;
                params.push(args[i].clone());
            }

            if entry.deferred {
                let slot = deferred.entry(arg.clone()).or_default();
                slot.extend(params);
            } else {
                (entry.handler)(&params, data).map_err(|e| {
                    format!(
                        "Error while reading option \"{}\" with value \"{}\"\nReason: {}",
                        arg, args[i], e
                    )
                })?;
            }

            i += 1;
        }

        // Run deferred handlers
        for (flag, params) in &deferred {
            let entry = options
                .get(flag.as_str())
                .ok_or_else(|| format!("Internal error: deferred option not found: {}", flag))?;
            (entry.handler)(params, data).map_err(|e| {
                let joined = if params.is_empty() {
                    "<none>".to_string()
                } else {
                    params.join(" ")
                };
                format!(
                    "Error while reading option \"{}\" with value \"{}\"\nReason: {}",
                    flag, joined, e
                )
            })?;
        }

        Ok(())
    }

    // -----------------------------------------------------------------------
    // Wrapper helpers for parameter validation
    // -----------------------------------------------------------------------

    /// Wraps a handler that takes no parameters.
    fn wrap_none<F>(flag: &str, f: F) -> HandlerFn
    where
        F: Fn(&mut ArgumentData) -> Result<(), String> + 'static,
    {
        let flag = flag.to_string();
        Box::new(move |params: &[String], data: &mut ArgumentData| {
            if !params.is_empty() {
                return Err(format!("Option \"{}\" does not accept parameters.", flag));
            }
            f(data)
        })
    }

    /// Wraps a handler that takes exactly one parameter.
    fn wrap_single<F>(flag: &str, f: F) -> HandlerFn
    where
        F: Fn(&str, &mut ArgumentData) -> Result<(), String> + 'static,
    {
        let flag = flag.to_string();
        Box::new(move |params: &[String], data: &mut ArgumentData| {
            if params.len() != 1 {
                return Err(format!("Option \"{}\" expects exactly one value.", flag));
            }
            f(&params[0], data)
        })
    }

    /// Wraps a handler that takes key=value pairs.
    fn wrap_key_value<F>(flag: &str, f: F) -> HandlerFn
    where
        F: Fn(&[(String, String)], &mut ArgumentData) -> Result<(), String> + 'static,
    {
        let flag = flag.to_string();
        Box::new(move |params: &[String], data: &mut ArgumentData| {
            if params.is_empty() {
                return Err(format!("Option \"{}\" expects key=value parameters.", flag));
            }

            let mut kv = Vec::new();
            for param in params {
                let pos = param.find('=');
                match pos {
                    Some(p) if p > 0 && p + 1 < param.len() => {
                        kv.push((param[..p].to_string(), param[p + 1..].to_string()));
                    }
                    _ => {
                        return Err(format!(
                            "Option \"{}\" expects key=value pairs, got \"{}\".",
                            flag, param
                        ));
                    }
                }
            }

            f(&kv, data)
        })
    }

    /// Wraps a handler that takes free-form parameters.
    fn wrap_free<F>(f: F) -> HandlerFn
    where
        F: Fn(&[String], &mut ArgumentData) -> Result<(), String> + 'static,
    {
        Box::new(move |params: &[String], data: &mut ArgumentData| f(params, data))
    }

    // -----------------------------------------------------------------------
    // Option registration
    // -----------------------------------------------------------------------

    fn register_options() -> HashMap<String, OptionEntry> {
        let mut opts: HashMap<String, OptionEntry> = HashMap::new();

        let add = |opts: &mut HashMap<String, OptionEntry>,
                   name: &str,
                   handler: HandlerFn,
                   deferred: bool| {
            let flag = format!("-{}", name);
            opts.insert(flag, OptionEntry { handler, deferred });
        };

        // -engine cmd=X name=Y tc=Z ...
        add(
            &mut opts,
            "engine",
            Self::wrap_key_value("-engine", |kv, data| {
                data.configs.push(EngineConfiguration::default());
                let idx = data.configs.len() - 1;
                for (key, value) in kv {
                    parse_engine_key_value(&mut data.configs[idx], key, value)?;
                }
                Ok(())
            }),
            false,
        );

        // -each (deferred — applied to all engines after all -engine flags)
        add(
            &mut opts,
            "each",
            Self::wrap_key_value("-each", |kv, data| {
                for (key, value) in kv {
                    for config in &mut data.configs {
                        parse_engine_key_value(config, key, value)?;
                    }
                }
                Ok(())
            }),
            true,
        );

        // -pgnout
        add(
            &mut opts,
            "pgnout",
            Self::wrap_key_value("-pgnout", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "file" => data.tournament_config.pgn.file = value.clone(),
                        "append" if is_bool(value) => {
                            data.tournament_config.pgn.append_file = value == "true"
                        }
                        "nodes" if is_bool(value) => {
                            data.tournament_config.pgn.track_nodes = value == "true"
                        }
                        "seldepth" if is_bool(value) => {
                            data.tournament_config.pgn.track_seldepth = value == "true"
                        }
                        "nps" if is_bool(value) => {
                            data.tournament_config.pgn.track_nps = value == "true"
                        }
                        "hashfull" if is_bool(value) => {
                            data.tournament_config.pgn.track_hashfull = value == "true"
                        }
                        "tbhits" if is_bool(value) => {
                            data.tournament_config.pgn.track_tbhits = value == "true"
                        }
                        "timeleft" if is_bool(value) => {
                            data.tournament_config.pgn.track_timeleft = value == "true"
                        }
                        "latency" if is_bool(value) => {
                            data.tournament_config.pgn.track_latency = value == "true"
                        }
                        "min" if is_bool(value) => data.tournament_config.pgn.min = value == "true",
                        "pv" if is_bool(value) => {
                            data.tournament_config.pgn.track_pv = value == "true"
                        }
                        "notation" => match value.as_str() {
                            "san" => data.tournament_config.pgn.notation = NotationType::San,
                            "lan" => data.tournament_config.pgn.notation = NotationType::Lan,
                            "uci" => data.tournament_config.pgn.notation = NotationType::Uci,
                            _ => {
                                return Err(format!(
                                    "Unrecognized pgnout notation option \"{}\" with value \"{}\".",
                                    key, value
                                ))
                            }
                        },
                        "match_line" => {
                            data.tournament_config
                                .pgn
                                .additional_lines_rgx
                                .push(value.clone());
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized pgnout option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                if data.tournament_config.pgn.file.is_empty() {
                    return Err("Please specify filename for pgn output.".to_string());
                }
                Ok(())
            }),
            false,
        );

        // -epdout
        add(
            &mut opts,
            "epdout",
            Self::wrap_key_value("-epdout", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "file" => data.tournament_config.epd.file = value.clone(),
                        "append" if is_bool(value) => {
                            data.tournament_config.epd.append_file = value == "true"
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized epdout option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                if data.tournament_config.epd.file.is_empty() {
                    return Err("Please specify filename for epd output.".to_string());
                }
                Ok(())
            }),
            false,
        );

        // -openings
        add(
            &mut opts,
            "openings",
            Self::wrap_key_value("-openings", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "file" => {
                            data.tournament_config.opening.file = value.clone();
                            if value.ends_with(".epd") {
                                data.tournament_config.opening.format = FormatType::Epd;
                            } else if value.ends_with(".pgn") {
                                data.tournament_config.opening.format = FormatType::Pgn;
                            }
                            if !std::path::Path::new(value).exists() {
                                return Err(format!("Opening file does not exist: {}", value));
                            }
                        }
                        "format" => match value.as_str() {
                            "epd" => data.tournament_config.opening.format = FormatType::Epd,
                            "pgn" => data.tournament_config.opening.format = FormatType::Pgn,
                            _ => {
                                return Err(format!(
                                    "Unrecognized openings format option \"{}\" with value \"{}\".",
                                    key, value
                                ))
                            }
                        },
                        "order" => match value.as_str() {
                            "sequential" => {
                                data.tournament_config.opening.order = OrderType::Sequential
                            }
                            "random" => data.tournament_config.opening.order = OrderType::Random,
                            _ => {
                                return Err(format!(
                                    "Unrecognized openings order option \"{}\" with value \"{}\".",
                                    key, value
                                ))
                            }
                        },
                        "plies" => {
                            data.tournament_config.opening.plies = value
                                .parse()
                                .map_err(|_| format!("Invalid plies value: {}", value))?;
                        }
                        "start" => {
                            let start: usize = value
                                .parse()
                                .map_err(|_| format!("Invalid start value: {}", value))?;
                            if start < 1 {
                                return Err("Starting offset must be at least 1!".to_string());
                            }
                            data.tournament_config.opening.start = start;
                        }
                        "policy" => {
                            if value != "round" {
                                return Err("Unsupported opening book policy.".to_string());
                            }
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized openings option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -sprt
        add(
            &mut opts,
            "sprt",
            Self::wrap_key_value("-sprt", |kv, data| {
                data.tournament_config.sprt.enabled = true;
                if data.tournament_config.rounds == 0 {
                    data.tournament_config.rounds = 500000;
                }
                for (key, value) in kv {
                    match key.as_str() {
                        "elo0" => {
                            data.tournament_config.sprt.elo0 = value
                                .parse()
                                .map_err(|_| format!("Invalid elo0 value: {}", value))?;
                        }
                        "elo1" => {
                            data.tournament_config.sprt.elo1 = value
                                .parse()
                                .map_err(|_| format!("Invalid elo1 value: {}", value))?;
                        }
                        "alpha" => {
                            data.tournament_config.sprt.alpha = value
                                .parse()
                                .map_err(|_| format!("Invalid alpha value: {}", value))?;
                        }
                        "beta" => {
                            data.tournament_config.sprt.beta = value
                                .parse()
                                .map_err(|_| format!("Invalid beta value: {}", value))?;
                        }
                        "model" => {
                            data.tournament_config.sprt.model = value.clone();
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized sprt option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -draw
        add(
            &mut opts,
            "draw",
            Self::wrap_key_value("-draw", |kv, data| {
                data.tournament_config.draw.enabled = true;
                for (key, value) in kv {
                    match key.as_str() {
                        "movenumber" => {
                            data.tournament_config.draw.move_number = value
                                .parse()
                                .map_err(|_| format!("Invalid movenumber value: {}", value))?;
                        }
                        "movecount" => {
                            data.tournament_config.draw.move_count = value
                                .parse()
                                .map_err(|_| format!("Invalid movecount value: {}", value))?;
                        }
                        "score" => {
                            let s: i32 = value
                                .parse()
                                .map_err(|_| format!("Invalid score value: {}", value))?;
                            if s < 0 {
                                return Err("Score cannot be negative.".to_string());
                            }
                            data.tournament_config.draw.score = s;
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized draw option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -resign
        add(
            &mut opts,
            "resign",
            Self::wrap_key_value("-resign", |kv, data| {
                data.tournament_config.resign.enabled = true;
                for (key, value) in kv {
                    match key.as_str() {
                        "movecount" => {
                            data.tournament_config.resign.move_count = value
                                .parse()
                                .map_err(|_| format!("Invalid movecount value: {}", value))?;
                        }
                        "twosided" if is_bool(value) => {
                            data.tournament_config.resign.twosided = value == "true";
                        }
                        "score" => {
                            let s: i32 = value
                                .parse()
                                .map_err(|_| format!("Invalid score value: {}", value))?;
                            if s < 0 {
                                return Err("Score cannot be negative.".to_string());
                            }
                            data.tournament_config.resign.score = s;
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized resign option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -maxmoves
        add(
            &mut opts,
            "maxmoves",
            Self::wrap_single("-maxmoves", |value, data| {
                data.tournament_config.maxmoves.move_count = value
                    .parse()
                    .map_err(|_| format!("Invalid maxmoves value: {}", value))?;
                data.tournament_config.maxmoves.enabled = true;
                Ok(())
            }),
            false,
        );

        // -tb
        add(
            &mut opts,
            "tb",
            Self::wrap_single("-tb", |value, data| {
                data.tournament_config.tb_adjudication.syzygy_dirs = value.to_string();
                data.tournament_config.tb_adjudication.enabled = true;
                Ok(())
            }),
            false,
        );

        // -tbpieces
        add(
            &mut opts,
            "tbpieces",
            Self::wrap_single("-tbpieces", |value, data| {
                data.tournament_config.tb_adjudication.max_pieces = value
                    .parse()
                    .map_err(|_| format!("Invalid tbpieces value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -tbignore50
        add(
            &mut opts,
            "tbignore50",
            Self::wrap_none("-tbignore50", |data| {
                data.tournament_config.tb_adjudication.ignore_50_move_rule = true;
                Ok(())
            }),
            false,
        );

        // -tbadjudicate
        add(
            &mut opts,
            "tbadjudicate",
            Self::wrap_single("-tbadjudicate", |value, data| {
                match value {
                    "WIN_LOSS" => {
                        data.tournament_config.tb_adjudication.result_type = TbResultType::WinLoss
                    }
                    "DRAW" => {
                        data.tournament_config.tb_adjudication.result_type = TbResultType::Draw
                    }
                    "BOTH" => {
                        data.tournament_config.tb_adjudication.result_type = TbResultType::Both
                    }
                    _ => return Err(format!("Invalid tb adjudication type: {}", value)),
                }
                Ok(())
            }),
            false,
        );

        // -autosaveinterval
        add(
            &mut opts,
            "autosaveinterval",
            Self::wrap_single("-autosaveinterval", |value, data| {
                data.tournament_config.autosaveinterval = value
                    .parse()
                    .map_err(|_| format!("Invalid autosaveinterval value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -log
        add(
            &mut opts,
            "log",
            Self::wrap_key_value("-log", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "file" => data.tournament_config.log.file = value.clone(),
                        "level" => {
                            data.tournament_config.log.level = match value.as_str() {
                                "trace" => LogLevel::Trace,
                                "info" => LogLevel::Info,
                                "warn" => LogLevel::Warn,
                                "err" => LogLevel::Error,
                                "fatal" => LogLevel::Fatal,
                                _ => {
                                    return Err(format!(
                                        "Unrecognized log level option \"{}\" with value \"{}\".",
                                        key, value
                                    ))
                                }
                            };
                        }
                        "append" if is_bool(value) => {
                            data.tournament_config.log.append_file = value == "true"
                        }
                        "compress" if is_bool(value) => {
                            data.tournament_config.log.compress = value == "true"
                        }
                        "realtime" if is_bool(value) => {
                            data.tournament_config.log.realtime = value == "true"
                        }
                        "engine" if is_bool(value) => {
                            data.tournament_config.log.engine_coms = value == "true"
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized log option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -config
        add(
            &mut opts,
            "config",
            Self::wrap_key_value("-config", |kv, data| {
                let mut drop_stats = false;
                for (key, value) in kv {
                    match key.as_str() {
                        "file" => load_json(data, value)?,
                        "outname" => data.tournament_config.config_name = value.clone(),
                        "discard" if value == "true" => {
                            log_info!("Discarding config file");
                            data.tournament_config = data.old_tournament_config.clone();
                            data.configs = data.old_configs.clone();
                            data.stats.clear();
                        }
                        "stats" => drop_stats = value == "false",
                        _ => {
                            return Err(format!(
                                "Unrecognized config option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                if data.configs.len() > 2 {
                    log_warn!("Warning: Stats will be dropped for more than 2 engines.");
                    data.stats.clear();
                }
                if drop_stats {
                    data.stats.clear();
                }
                Ok(())
            }),
            false,
        );

        // -report
        add(
            &mut opts,
            "report",
            Self::wrap_key_value("-report", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "penta" if is_bool(value) => {
                            data.tournament_config.report_penta = value == "true"
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized report option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -output
        add(
            &mut opts,
            "output",
            Self::wrap_key_value("-output", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "format" => match value.as_str() {
                            "cutechess" => data.tournament_config.output = OutputType::Cutechess,
                            "fastchess" => data.tournament_config.output = OutputType::Fastchess,
                            _ => {
                                return Err(format!(
                                    "Unrecognized output format option \"{}\" with value \"{}\".",
                                    key, value
                                ))
                            }
                        },
                        _ => {
                            return Err(format!(
                                "Unrecognized output option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -concurrency
        add(
            &mut opts,
            "concurrency",
            Self::wrap_single("-concurrency", |value, data| {
                // Parse as i32 to support negative values (e.g., -concurrency -2)
                let concurrency_raw: i32 = value
                    .parse()
                    .map_err(|_| format!("Invalid concurrency value: {}", value))?;

                // Negative or zero means "hardware_threads - abs(value)"
                // This matches the C++ behavior: -concurrency -2 means "use all threads except 2"
                if concurrency_raw <= 0 {
                    let hw_threads = std::thread::available_parallelism()
                        .map(|n| n.get())
                        .unwrap_or(1);

                    let mut adjusted = hw_threads as i32 - concurrency_raw.abs();

                    if adjusted < 0 {
                        log_warn!(
                            "Warning: Adjusted concurrency value {} is less than 0, setting to 1.",
                            adjusted
                        );
                        adjusted = 1;
                    }

                    data.tournament_config.concurrency = adjusted as usize;
                    println!(
                        "Info: Adjusted concurrency to {} based on number of available hardware threads.",
                        data.tournament_config.concurrency
                    );
                } else {
                    data.tournament_config.concurrency = concurrency_raw as usize;
                }
                Ok(())
            }),
            false,
        );

        // -crc32
        add(
            &mut opts,
            "crc32",
            Self::wrap_key_value("-crc32", |kv, data| {
                for (key, value) in kv {
                    match key.as_str() {
                        "pgn" if is_bool(value) => data.tournament_config.pgn.crc = value == "true",
                        _ => {
                            return Err(format!(
                                "Unrecognized crc option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }
                Ok(())
            }),
            false,
        );

        // -force-concurrency
        add(
            &mut opts,
            "force-concurrency",
            Self::wrap_none("-force-concurrency", |data| {
                data.tournament_config.force_concurrency = true;
                Ok(())
            }),
            false,
        );

        // -event (free)
        add(
            &mut opts,
            "event",
            Self::wrap_free(|params, data| {
                data.tournament_config.pgn.event_name = params.join("");
                Ok(())
            }),
            false,
        );

        // -site (free)
        add(
            &mut opts,
            "site",
            Self::wrap_free(|params, data| {
                data.tournament_config.pgn.site = params.join("");
                Ok(())
            }),
            false,
        );

        // -games
        add(
            &mut opts,
            "games",
            Self::wrap_single("-games", |value, data| {
                data.tournament_config.games = value
                    .parse()
                    .map_err(|_| format!("Invalid games value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -rounds
        add(
            &mut opts,
            "rounds",
            Self::wrap_single("-rounds", |value, data| {
                data.tournament_config.rounds = value
                    .parse()
                    .map_err(|_| format!("Invalid rounds value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -wait
        add(
            &mut opts,
            "wait",
            Self::wrap_single("-wait", |value, data| {
                data.tournament_config.wait = value
                    .parse()
                    .map_err(|_| format!("Invalid wait value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -noswap
        add(
            &mut opts,
            "noswap",
            Self::wrap_none("-noswap", |data| {
                data.tournament_config.noswap = true;
                Ok(())
            }),
            false,
        );

        // -reverse
        add(
            &mut opts,
            "reverse",
            Self::wrap_none("-reverse", |data| {
                data.tournament_config.reverse = true;
                Ok(())
            }),
            false,
        );

        // -ratinginterval
        add(
            &mut opts,
            "ratinginterval",
            Self::wrap_single("-ratinginterval", |value, data| {
                data.tournament_config.ratinginterval = value
                    .parse()
                    .map_err(|_| format!("Invalid ratinginterval value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -scoreinterval
        add(
            &mut opts,
            "scoreinterval",
            Self::wrap_single("-scoreinterval", |value, data| {
                data.tournament_config.scoreinterval = value
                    .parse()
                    .map_err(|_| format!("Invalid scoreinterval value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -srand
        add(
            &mut opts,
            "srand",
            Self::wrap_single("-srand", |value, data| {
                data.tournament_config.seed = value
                    .parse()
                    .map_err(|_| format!("Invalid srand value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -seeds
        add(
            &mut opts,
            "seeds",
            Self::wrap_single("-seeds", |value, data| {
                data.tournament_config.gauntlet_seeds = value
                    .parse()
                    .map_err(|_| format!("Invalid seeds value: {}", value))?;
                Ok(())
            }),
            false,
        );

        // -version, --version, -v, --v
        let version_handler = |_data: &mut ArgumentData| -> Result<(), String> {
            println!("fastchess-rust {}", env!("CARGO_PKG_VERSION"));
            std::process::exit(0);
        };
        for name in &["version", "-version", "v", "-v"] {
            add(
                &mut opts,
                name,
                Self::wrap_none(&format!("-{}", name), version_handler),
                false,
            );
        }

        // -help, --help
        for name in &["help", "-help"] {
            add(
                &mut opts,
                name,
                Self::wrap_none(&format!("-{}", name), |_data| {
                    print_help();
                    std::process::exit(0);
                }),
                false,
            );
        }

        // -recover
        add(
            &mut opts,
            "recover",
            Self::wrap_none("-recover", |data| {
                data.tournament_config.recover = true;
                Ok(())
            }),
            false,
        );

        // -repeat (free — optional numeric argument)
        add(
            &mut opts,
            "repeat",
            Self::wrap_free(|params, data| {
                if params.len() == 1 && params[0].chars().all(|c| c.is_ascii_digit()) {
                    data.tournament_config.games = params[0]
                        .parse()
                        .map_err(|_| format!("Invalid repeat value: {}", params[0]))?;
                } else {
                    data.tournament_config.games = 2;
                }
                Ok(())
            }),
            false,
        );

        // -variant
        add(
            &mut opts,
            "variant",
            Self::wrap_single("-variant", |value, data| {
                match value {
                    "fischerandom" => data.tournament_config.variant = VariantType::Frc,
                    "standard" => data.tournament_config.variant = VariantType::Standard,
                    "shogi" => data.tournament_config.variant = VariantType::Shogi,
                    _ => {
                        return Err(
                            "Unknown variant. Use: standard, fischerandom, or shogi.".to_string()
                        )
                    }
                }
                Ok(())
            }),
            false,
        );

        // -tournament
        add(
            &mut opts,
            "tournament",
            Self::wrap_single("-tournament", |value, data| {
                match value {
                    "gauntlet" => data.tournament_config.r#type = TournamentType::Gauntlet,
                    "roundrobin" => data.tournament_config.r#type = TournamentType::RoundRobin,
                    _ => {
                        return Err(
                            "Unsupported tournament format. Only supports roundrobin and gauntlet."
                                .to_string(),
                        )
                    }
                }
                Ok(())
            }),
            false,
        );

        // -quick
        add(
            &mut opts,
            "quick",
            Self::wrap_key_value("-quick", |kv, data| {
                let initial_index = data.configs.len();
                let mut engine_count = 0;

                for (key, value) in kv {
                    match key.as_str() {
                        "cmd" => {
                            let mut config = EngineConfiguration {
                                cmd: value.clone(),
                                name: value.clone(),
                                ..Default::default()
                            };
                            config.limit.tc.time = 10 * 1000;
                            config.limit.tc.increment = 100;
                            data.configs.push(config);
                            data.tournament_config.recover = true;
                            engine_count += 1;
                        }
                        "book" => {
                            data.tournament_config.opening.file = value.clone();
                            data.tournament_config.opening.order = OrderType::Random;
                            if value.ends_with(".pgn") {
                                data.tournament_config.opening.format = FormatType::Pgn;
                            } else if value.ends_with(".epd") {
                                data.tournament_config.opening.format = FormatType::Epd;
                            } else {
                                return Err(
                                    "Please include the .pgn or .epd file extension for the opening book."
                                        .to_string(),
                                );
                            }
                        }
                        _ => {
                            return Err(format!(
                                "Unrecognized quick option \"{}\" with value \"{}\".",
                                key, value
                            ))
                        }
                    }
                }

                if engine_count != 2 {
                    return Err("Option \"-quick\" requires exactly two cmd entries.".to_string());
                }

                if data.tournament_config.opening.file.is_empty() {
                    return Err("Option \"-quick\" requires a book=FILE entry.".to_string());
                }

                // Disambiguate names
                if data.configs[initial_index].name == data.configs[initial_index + 1].name {
                    data.configs[initial_index].name.push('1');
                    data.configs[initial_index + 1].name.push('2');
                }

                data.tournament_config.games = 2;
                data.tournament_config.rounds = 25000;

                let hw_threads = std::thread::available_parallelism()
                    .map(|n| n.get())
                    .unwrap_or(1);
                data.tournament_config.concurrency = hw_threads.saturating_sub(2).max(1);
                data.tournament_config.recover = true;

                data.tournament_config.draw.enabled = true;
                data.tournament_config.draw.move_number = 30;
                data.tournament_config.draw.move_count = 8;
                data.tournament_config.draw.score = 8;

                data.tournament_config.output = OutputType::Cutechess;

                Ok(())
            }),
            false,
        );

        // -use-affinity (free — optional cpu list)
        add(
            &mut opts,
            "use-affinity",
            Self::wrap_free(|params, data| {
                data.tournament_config.affinity = true;
                if !params.is_empty() {
                    data.tournament_config.affinity_cpus = parse_int_list(&params[0])?;
                }
                Ok(())
            }),
            false,
        );

        // -show-latency
        add(
            &mut opts,
            "show-latency",
            Self::wrap_none("-show-latency", |data| {
                data.tournament_config.show_latency = true;
                Ok(())
            }),
            false,
        );

        // -debug (error — not supported)
        add(
            &mut opts,
            "debug",
            Self::wrap_none("-debug", |_data| {
                Err("The 'debug' option does not exist in fastchess. \
                     Use the 'log' option instead to write all engine input \
                     and output into a text file."
                    .to_string())
            }),
            false,
        );

        // -testEnv
        add(
            &mut opts,
            "testEnv",
            Self::wrap_none("-testEnv", |data| {
                data.tournament_config.test_env = true;
                Ok(())
            }),
            false,
        );

        // -startup-ms
        add(
            &mut opts,
            "startup-ms",
            Self::wrap_single("-startup-ms", |value, data| {
                let ms: u64 = value
                    .parse()
                    .map_err(|_| format!("Invalid startup-ms value: {}", value))?;
                data.tournament_config.startup_time = Duration::from_millis(ms);
                Ok(())
            }),
            false,
        );

        // -ucinewgame-ms
        add(
            &mut opts,
            "ucinewgame-ms",
            Self::wrap_single("-ucinewgame-ms", |value, data| {
                let ms: u64 = value
                    .parse()
                    .map_err(|_| format!("Invalid ucinewgame-ms value: {}", value))?;
                data.tournament_config.ucinewgame_time = Duration::from_millis(ms);
                Ok(())
            }),
            false,
        );

        // -ping-ms
        add(
            &mut opts,
            "ping-ms",
            Self::wrap_single("-ping-ms", |value, data| {
                let ms: u64 = value
                    .parse()
                    .map_err(|_| format!("Invalid ping-ms value: {}", value))?;
                data.tournament_config.ping_time = Duration::from_millis(ms);
                Ok(())
            }),
            false,
        );

        opts
    }
}

// ---------------------------------------------------------------------------
// Engine key=value parsing
// ---------------------------------------------------------------------------

/// Parse a single key=value pair for an engine configuration.
fn parse_engine_key_value(
    config: &mut EngineConfiguration,
    key: &str,
    value: &str,
) -> Result<(), String> {
    match key {
        "cmd" => config.cmd = value.to_string(),
        "name" => config.name = value.to_string(),
        "tc" => config.limit.tc = parse_tc(value)?,
        "st" => {
            config.limit.tc.fixed_time = (value
                .parse::<f64>()
                .map_err(|_| format!("Invalid st value: {}", value))?
                * 1000.0) as i64;
        }
        "timemargin" => {
            let margin: i64 = value
                .parse()
                .map_err(|_| format!("Invalid timemargin value: {}", value))?;
            if margin < 0 {
                return Err("The value for timemargin cannot be a negative number.".to_string());
            }
            config.limit.tc.timemargin = margin;
        }
        "nodes" => {
            config.limit.nodes = value
                .parse()
                .map_err(|_| format!("Invalid nodes value: {}", value))?;
        }
        "plies" | "depth" => {
            config.limit.plies = value
                .parse()
                .map_err(|_| format!("Invalid plies/depth value: {}", value))?;
        }
        "dir" => config.dir = value.to_string(),
        "args" => config.args = value.to_string(),
        "restart" => match value {
            "on" => config.restart = true,
            "off" => config.restart = false,
            _ => {
                return Err(format!(
                    "Invalid parameter (must be either \"on\" or \"off\"): {}",
                    value
                ))
            }
        },
        "proto" => {
            if value != "uci" {
                return Err("Unsupported protocol.".to_string());
            }
        }
        _ if key.starts_with("option.") => {
            let option_name = &key["option.".len()..];
            config
                .options
                .push((option_name.to_string(), value.to_string()));
        }
        _ => {
            return Err(format!(
                "Unrecognized engine option \"{}\" with value \"{}\".",
                key, value
            ))
        }
    }
    Ok(())
}

// ---------------------------------------------------------------------------
// Time control parsing
// ---------------------------------------------------------------------------

/// Parse a time control string like `moves/minutes:seconds+increment`.
///
/// Examples:
/// - `40/60` — 40 moves in 60 seconds
/// - `0:30+0.1` — 30 seconds + 0.1s increment
/// - `10+0.1` — 10 seconds + 0.1s increment
/// - `2+0.02s` — 2 seconds + 0.02s increment (with 's' suffix)
/// - `infinite` / `inf` — no time limit
fn parse_tc(tc_string: &str) -> Result<TimeControlLimits, String> {
    if tc_string.contains("hg") {
        return Err("Hourglass time control not supported.".to_string());
    }

    if tc_string == "infinite" || tc_string == "inf" {
        return Ok(TimeControlLimits::default());
    }

    let mut tc = TimeControlLimits::default();
    let mut remaining = tc_string.to_string();

    // Check for moves/... prefix
    if let Some(slash_pos) = remaining.find('/') {
        let moves_str = &remaining[..slash_pos];
        if moves_str == "inf" || moves_str == "infinite" {
            tc.moves = 0;
        } else {
            tc.moves = moves_str
                .parse()
                .map_err(|_| format!("Invalid moves value in tc: {}", moves_str))?;
        }
        remaining = remaining[slash_pos + 1..].to_string();
    }

    // Check for +increment suffix
    if let Some(plus_pos) = remaining.find('+') {
        let inc_str = &remaining[plus_pos + 1..];
        // Strip trailing 's' suffix if present (e.g., "0.02s" -> "0.02")
        let inc_str = inc_str.strip_suffix('s').unwrap_or(inc_str);
        let inc_secs: f64 = inc_str
            .parse()
            .map_err(|_| format!("Invalid increment value in tc: {}", inc_str))?;
        tc.increment = (inc_secs * 1000.0) as i64;
        remaining = remaining[..plus_pos].to_string();
    }

    // Strip trailing 's' suffix from main time if present
    remaining = remaining
        .strip_suffix('s')
        .unwrap_or(&remaining)
        .to_string();

    // Check for minutes:seconds format
    if remaining.contains(':') {
        let parts: Vec<&str> = remaining.split(':').collect();
        if parts.len() != 2 {
            return Err(format!("Invalid time format: {}", remaining));
        }
        let minutes: f64 = parts[0]
            .parse()
            .map_err(|_| format!("Invalid minutes value: {}", parts[0]))?;
        let seconds: f64 = parts[1]
            .parse()
            .map_err(|_| format!("Invalid seconds value: {}", parts[1]))?;
        tc.time = (minutes * 1000.0 * 60.0) as i64 + (seconds * 1000.0) as i64;
    } else {
        let seconds: f64 = remaining
            .parse()
            .map_err(|_| format!("Invalid time value: {}", remaining))?;
        tc.time = (seconds * 1000.0) as i64;
    }

    Ok(tc)
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn is_bool(s: &str) -> bool {
    s == "true" || s == "false"
}

/// Parse a comma-separated list of integers, supporting ranges with dashes.
///
/// Example: `"5,10,13-17,23"` → `[5, 10, 13, 14, 15, 16, 17, 23]`
fn parse_int_list(s: &str) -> Result<Vec<i32>, String> {
    let mut result = Vec::new();

    for part in s.split(',') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }

        if let Some(dash_pos) = part.find('-') {
            // Handle the case where dash is at position 0 (negative number, not a range)
            if dash_pos == 0 {
                let n: i32 = part
                    .parse()
                    .map_err(|_| format!("Bad cpu list: invalid number '{}'", part))?;
                result.push(n);
                continue;
            }

            let start: i32 = part[..dash_pos].parse().map_err(|_| {
                format!("Bad cpu list: invalid range start '{}'", &part[..dash_pos])
            })?;
            let end: i32 = part[dash_pos + 1..].parse().map_err(|_| {
                format!(
                    "Bad cpu list: invalid range end '{}'",
                    &part[dash_pos + 1..]
                )
            })?;

            if end <= start {
                return Err(format!("Bad cpu list: invalid range {}-{}", start, end));
            }

            for i in start..=end {
                result.push(i);
            }
        } else {
            let n: i32 = part
                .parse()
                .map_err(|_| format!("Bad cpu list: invalid number '{}'", part))?;
            result.push(n);
        }
    }

    Ok(result)
}

/// Load tournament configuration from a JSON file.
fn load_json(data: &mut ArgumentData, filename: &str) -> Result<(), String> {
    log_info!("Loading config file: {}", filename);

    let contents = std::fs::read_to_string(filename)
        .map_err(|e| format!("File not found: {} ({})", filename, e))?;

    let json_value: serde_json::Value =
        serde_json::from_str(&contents).map_err(|e| format!("JSON parse error: {}", e))?;

    data.old_configs = data.configs.clone();
    data.old_tournament_config = data.tournament_config.clone();

    data.tournament_config = serde_json::from_value(json_value.clone())
        .map_err(|e| format!("Failed to parse tournament config: {}", e))?;

    data.configs.clear();
    if let Some(engines) = json_value.get("engines") {
        if let Some(arr) = engines.as_array() {
            for engine in arr {
                let config: EngineConfiguration = serde_json::from_value(engine.clone())
                    .map_err(|e| format!("Failed to parse engine config: {}", e))?;
                data.configs.push(config);
            }
        }
    }

    if let Some(stats) = json_value.get("stats") {
        data.stats = crate::matchmaking::scoreboard::stats_map_from_json(stats);
    }

    Ok(())
}

/// Print help text.
fn print_help() {
    println!(
        "fastchess-rust {}\n\
         \n\
         Usage: fastchess [OPTIONS]\n\
         \n\
         Engine options:\n\
         -engine cmd=<path> name=<name> [tc=<tc>] [st=<time>] [nodes=<n>] [plies=<n>]\n\
             [dir=<path>] [args=<args>] [restart=on|off] [option.<name>=<value>]\n\
         -each <key=value pairs>   Apply options to all engines\n\
         \n\
         Tournament options:\n\
         -tournament <roundrobin|gauntlet>\n\
         -concurrency <n>          Number of concurrent games\n\
         -rounds <n>               Number of rounds\n\
         -games <n>                Games per encounter (1 or 2)\n\
         -repeat [n]               Shorthand for -games n (default 2)\n\
         -noswap                   Do not swap colors\n\
         -reverse                  Reverse color assignment\n\
         -recover                  Continue after engine crash\n\
         -variant <standard|fischerandom>\n\
         -seeds <n>                Number of gauntlet seeds\n\
         \n\
         Time control:\n\
         tc format: [moves/]time[+increment]\n\
             time can be seconds or minutes:seconds\n\
             Example: 40/60+0.6, 0:30+0.1, 10+0.1\n\
         st=<seconds>              Fixed time per move\n\
         \n\
         Adjudication:\n\
         -draw movenumber=<n> movecount=<n> score=<n>\n\
         -resign movecount=<n> score=<n> [twosided=true|false]\n\
         -maxmoves <n>\n\
         -tb <syzygy_dirs> [-tbpieces <n>] [-tbignore50] [-tbadjudicate <WIN_LOSS|DRAW|BOTH>]\n\
         \n\
         Output:\n\
         -pgnout file=<path> [notation=san|lan|uci] [append=true|false] ...\n\
         -epdout file=<path> [append=true|false]\n\
         -output format=<fastchess|cutechess>\n\
         -report penta=<true|false>\n\
         -ratinginterval <n>\n\
         -scoreinterval <n>\n\
         \n\
         SPRT:\n\
         -sprt elo0=<n> elo1=<n> [alpha=<n>] [beta=<n>] [model=<name>]\n\
         \n\
         Opening book:\n\
         -openings file=<path> [format=epd|pgn] [order=sequential|random] [plies=<n>] [start=<n>]\n\
         \n\
         Misc:\n\
         -log file=<path> [level=trace|info|warn|err|fatal] [engine=true|false] ...\n\
         -config file=<path> [outname=<name>] [discard=true] [stats=false]\n\
         -crc32 pgn=true|false\n\
         -use-affinity [cpu_list]  CPU pinning (e.g. 0,2,4-7)\n\
         -show-latency\n\
         -srand <seed>\n\
         -autosaveinterval <n>\n\
         -wait <seconds>\n\
         -force-concurrency\n\
         -quick cmd=<path1> cmd=<path2> book=<file>\n\
         -startup-ms <ms>\n\
         -ucinewgame-ms <ms>\n\
         -ping-ms <ms>\n\
         -version, -v\n\
         -help\n",
        env!("CARGO_PKG_VERSION")
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_tc_basic() {
        let tc = parse_tc("10").unwrap();
        assert_eq!(tc.time, 10000);
        assert_eq!(tc.increment, 0);
        assert_eq!(tc.moves, 0);
    }

    #[test]
    fn test_parse_tc_with_increment() {
        let tc = parse_tc("10+0.1").unwrap();
        assert_eq!(tc.time, 10000);
        assert_eq!(tc.increment, 100);
    }

    #[test]
    fn test_parse_tc_with_minutes() {
        let tc = parse_tc("1:30").unwrap();
        assert_eq!(tc.time, 90000);
    }

    #[test]
    fn test_parse_tc_with_moves() {
        let tc = parse_tc("40/60").unwrap();
        assert_eq!(tc.moves, 40);
        assert_eq!(tc.time, 60000);
    }

    #[test]
    fn test_parse_tc_full() {
        let tc = parse_tc("40/1:30+0.5").unwrap();
        assert_eq!(tc.moves, 40);
        assert_eq!(tc.time, 90000);
        assert_eq!(tc.increment, 500);
    }

    #[test]
    fn test_parse_tc_infinite() {
        let tc = parse_tc("infinite").unwrap();
        assert_eq!(tc.time, 0);
        assert_eq!(tc.increment, 0);
    }

    #[test]
    fn test_parse_tc_with_s_suffix() {
        // increment with 's' suffix
        let tc = parse_tc("2+0.02s").unwrap();
        assert_eq!(tc.time, 2000);
        assert_eq!(tc.increment, 20);

        // main time with 's' suffix
        let tc = parse_tc("10s+0.1").unwrap();
        assert_eq!(tc.time, 10000);
        assert_eq!(tc.increment, 100);

        // both with 's' suffix
        let tc = parse_tc("5s+0.5s").unwrap();
        assert_eq!(tc.time, 5000);
        assert_eq!(tc.increment, 500);

        // main time only with 's' suffix
        let tc = parse_tc("30s").unwrap();
        assert_eq!(tc.time, 30000);
        assert_eq!(tc.increment, 0);
    }

    #[test]
    fn test_parse_int_list() {
        let list = parse_int_list("5,10,13-17,23").unwrap();
        assert_eq!(list, vec![5, 10, 13, 14, 15, 16, 17, 23]);
    }

    #[test]
    fn test_parse_int_list_single() {
        let list = parse_int_list("3").unwrap();
        assert_eq!(list, vec![3]);
    }

    #[test]
    fn test_parse_int_list_range() {
        let list = parse_int_list("0-3").unwrap();
        assert_eq!(list, vec![0, 1, 2, 3]);
    }

    #[test]
    fn test_parse_engine_key_values() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "cmd", "stockfish").unwrap();
        parse_engine_key_value(&mut config, "name", "SF").unwrap();
        parse_engine_key_value(&mut config, "tc", "10+0.1").unwrap();
        parse_engine_key_value(&mut config, "option.Hash", "128").unwrap();

        assert_eq!(config.cmd, "stockfish");
        assert_eq!(config.name, "SF");
        assert_eq!(config.limit.tc.time, 10000);
        assert_eq!(config.limit.tc.increment, 100);
        assert_eq!(
            config.options,
            vec![("Hash".to_string(), "128".to_string())]
        );
    }

    #[test]
    fn test_parse_engine_bad_option() {
        let mut config = EngineConfiguration::default();
        assert!(parse_engine_key_value(&mut config, "badkey", "value").is_err());
    }

    #[test]
    fn test_parse_engine_restart() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "restart", "on").unwrap();
        assert!(config.restart);
        parse_engine_key_value(&mut config, "restart", "off").unwrap();
        assert!(!config.restart);
        assert!(parse_engine_key_value(&mut config, "restart", "maybe").is_err());
    }

    #[test]
    fn test_full_parse_basic() {
        let args: Vec<String> = vec![
            "fastchess",
            "-engine",
            "cmd=stockfish",
            "name=SF1",
            "tc=10+0.1",
            "-engine",
            "cmd=stockfish",
            "name=SF2",
            "tc=10+0.1",
            "-rounds",
            "100",
            "-games",
            "2",
        ]
        .into_iter()
        .map(|s| s.to_string())
        .collect();

        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.engine_configs().len(), 2);
        assert_eq!(parser.engine_configs()[0].name, "SF1");
        assert_eq!(parser.engine_configs()[1].name, "SF2");
        assert_eq!(parser.tournament_config().rounds, 100);
        assert_eq!(parser.tournament_config().games, 2);
    }

    #[test]
    fn test_each_applies_to_all_engines() {
        let args: Vec<String> = vec![
            "fastchess",
            "-engine",
            "cmd=eng1",
            "name=E1",
            "-engine",
            "cmd=eng2",
            "name=E2",
            "-each",
            "tc=10+0.1",
            "-rounds",
            "1",
        ]
        .into_iter()
        .map(|s| s.to_string())
        .collect();

        let parser = OptionsParser::new(&args).unwrap();
        for config in parser.engine_configs() {
            assert_eq!(config.limit.tc.time, 10000);
            assert_eq!(config.limit.tc.increment, 100);
        }
    }

    #[test]
    fn test_unrecognized_option() {
        let args: Vec<String> = vec!["fastchess", "-bogus"]
            .into_iter()
            .map(|s| s.to_string())
            .collect();

        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_no_args() {
        let args: Vec<String> = vec!["fastchess"]
            .into_iter()
            .map(|s| s.to_string())
            .collect();

        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_negative_concurrency() {
        let args: Vec<String> = vec![
            "fastchess",
            "-engine",
            "cmd=eng1",
            "name=E1",
            "tc=1+0",
            "-engine",
            "cmd=eng2",
            "name=E2",
            "tc=1+0",
            "-concurrency",
            "-2",
            "-rounds",
            "1",
        ]
        .into_iter()
        .map(|s| s.to_string())
        .collect();

        let parser = OptionsParser::new(&args).unwrap();
        let hw_threads = std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(1);

        // -2 should become "hw_threads - 2", with a minimum of 1
        let expected = (hw_threads as i32 - 2).max(1) as usize;
        assert_eq!(parser.tournament_config().concurrency, expected);
    }

    #[test]
    fn test_zero_concurrency() {
        let args: Vec<String> = vec![
            "fastchess",
            "-engine",
            "cmd=eng1",
            "name=E1",
            "tc=1+0",
            "-engine",
            "cmd=eng2",
            "name=E2",
            "tc=1+0",
            "-concurrency",
            "0",
            "-rounds",
            "1",
        ]
        .into_iter()
        .map(|s| s.to_string())
        .collect();

        let parser = OptionsParser::new(&args).unwrap();
        let hw_threads = std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(1);

        // 0 should become "hw_threads - 0" = hw_threads
        assert_eq!(parser.tournament_config().concurrency, hw_threads);
    }

    /// Helper: build args with two engines + extra flags appended.
    fn make_args(extra: &[&str]) -> Vec<String> {
        let mut base: Vec<&str> = vec![
            "fastchess",
            "-engine",
            "cmd=eng1",
            "name=E1",
            "tc=1+0",
            "-engine",
            "cmd=eng2",
            "name=E2",
            "tc=1+0",
            "-rounds",
            "1",
        ];
        base.extend_from_slice(extra);
        base.into_iter().map(|s| s.to_string()).collect()
    }

    // -----------------------------------------------------------------------
    // Tournament options
    // -----------------------------------------------------------------------

    #[test]
    fn test_tournament_roundrobin() {
        let args = make_args(&["-tournament", "roundrobin"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().r#type,
            TournamentType::RoundRobin
        );
    }

    #[test]
    fn test_tournament_gauntlet() {
        let args = make_args(&["-tournament", "gauntlet"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().r#type, TournamentType::Gauntlet);
    }

    #[test]
    fn test_tournament_invalid() {
        let args = make_args(&["-tournament", "swiss"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_variant_standard() {
        let args = make_args(&["-variant", "standard"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().variant, VariantType::Standard);
    }

    #[test]
    fn test_variant_fischerandom() {
        // fischerandom requires an opening book for validation, so verify the error
        let args = make_args(&["-variant", "fischerandom"]);
        let result = OptionsParser::new(&args);
        assert!(result.is_err());
    }

    #[test]
    fn test_variant_shogi() {
        let args = make_args(&["-variant", "shogi"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().variant, VariantType::Shogi);
    }

    #[test]
    fn test_variant_invalid() {
        let args = make_args(&["-variant", "atomic"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_noswap() {
        let args = make_args(&["-noswap"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().noswap);
    }

    #[test]
    fn test_reverse() {
        let args = make_args(&["-reverse"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().reverse);
    }

    #[test]
    fn test_recover() {
        let args = make_args(&["-recover"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().recover);
    }

    #[test]
    fn test_games() {
        let args = make_args(&["-games", "1"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().games, 1);
    }

    #[test]
    fn test_repeat_default() {
        let args = make_args(&["-repeat"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().games, 2);
    }

    #[test]
    fn test_repeat_with_value() {
        // -repeat 2 sets games to 2 explicitly
        let args = make_args(&["-repeat", "2"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().games, 2);
    }

    #[test]
    fn test_seeds() {
        let args = make_args(&["-seeds", "3"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().gauntlet_seeds, 3);
    }

    #[test]
    fn test_wait() {
        let args = make_args(&["-wait", "5"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().wait, 5);
    }

    #[test]
    fn test_srand() {
        let args = make_args(&["-srand", "42"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().seed, 42);
    }

    #[test]
    fn test_concurrency_positive() {
        let args = make_args(&["-concurrency", "1"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().concurrency, 1);
    }

    #[test]
    fn test_force_concurrency() {
        let args = make_args(&["-force-concurrency"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().force_concurrency);
    }

    // -----------------------------------------------------------------------
    // Time control options
    // -----------------------------------------------------------------------

    #[test]
    fn test_startup_ms() {
        let args = make_args(&["-startup-ms", "5000"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().startup_time,
            Duration::from_millis(5000)
        );
    }

    #[test]
    fn test_ucinewgame_ms() {
        let args = make_args(&["-ucinewgame-ms", "30000"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().ucinewgame_time,
            Duration::from_millis(30000)
        );
    }

    #[test]
    fn test_ping_ms() {
        let args = make_args(&["-ping-ms", "120000"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().ping_time,
            Duration::from_millis(120000)
        );
    }

    // -----------------------------------------------------------------------
    // Adjudication options
    // -----------------------------------------------------------------------

    #[test]
    fn test_draw_adjudication() {
        let args = make_args(&["-draw", "movenumber=40", "movecount=8", "score=10"]);
        let parser = OptionsParser::new(&args).unwrap();
        let draw = &parser.tournament_config().draw;
        assert!(draw.enabled);
        assert_eq!(draw.move_number, 40);
        assert_eq!(draw.move_count, 8);
        assert_eq!(draw.score, 10);
    }

    #[test]
    fn test_draw_negative_score() {
        let args = make_args(&["-draw", "score=-5"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_draw_unrecognized_key() {
        let args = make_args(&["-draw", "badkey=1"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_resign_adjudication() {
        let args = make_args(&["-resign", "movecount=5", "score=600", "twosided=true"]);
        let parser = OptionsParser::new(&args).unwrap();
        let resign = &parser.tournament_config().resign;
        assert!(resign.enabled);
        assert_eq!(resign.move_count, 5);
        assert_eq!(resign.score, 600);
        assert!(resign.twosided);
    }

    #[test]
    fn test_resign_twosided_false() {
        let args = make_args(&["-resign", "movecount=3", "score=400", "twosided=false"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(!parser.tournament_config().resign.twosided);
    }

    #[test]
    fn test_resign_negative_score() {
        let args = make_args(&["-resign", "score=-100"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_maxmoves() {
        let args = make_args(&["-maxmoves", "200"]);
        let parser = OptionsParser::new(&args).unwrap();
        let mm = &parser.tournament_config().maxmoves;
        assert!(mm.enabled);
        assert_eq!(mm.move_count, 200);
    }

    #[test]
    fn test_tb() {
        let args = make_args(&["-tb", "/path/to/syzygy"]);
        let parser = OptionsParser::new(&args).unwrap();
        let tb = &parser.tournament_config().tb_adjudication;
        assert!(tb.enabled);
        assert_eq!(tb.syzygy_dirs, "/path/to/syzygy");
    }

    #[test]
    fn test_tbpieces() {
        let args = make_args(&["-tbpieces", "5"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().tb_adjudication.max_pieces, 5);
    }

    #[test]
    fn test_tbignore50() {
        let args = make_args(&["-tbignore50"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(
            parser
                .tournament_config()
                .tb_adjudication
                .ignore_50_move_rule
        );
    }

    #[test]
    fn test_tbadjudicate_win_loss() {
        let args = make_args(&["-tbadjudicate", "WIN_LOSS"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().tb_adjudication.result_type,
            TbResultType::WinLoss
        );
    }

    #[test]
    fn test_tbadjudicate_draw() {
        let args = make_args(&["-tbadjudicate", "DRAW"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().tb_adjudication.result_type,
            TbResultType::Draw
        );
    }

    #[test]
    fn test_tbadjudicate_both() {
        let args = make_args(&["-tbadjudicate", "BOTH"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().tb_adjudication.result_type,
            TbResultType::Both
        );
    }

    #[test]
    fn test_tbadjudicate_invalid() {
        let args = make_args(&["-tbadjudicate", "INVALID"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    // -----------------------------------------------------------------------
    // SPRT options
    // -----------------------------------------------------------------------

    #[test]
    fn test_sprt() {
        let args = make_args(&[
            "-sprt",
            "elo0=0",
            "elo1=5",
            "alpha=0.05",
            "beta=0.05",
            "model=logistic",
        ]);
        let parser = OptionsParser::new(&args).unwrap();
        let sprt = &parser.tournament_config().sprt;
        assert!(sprt.enabled);
        assert!((sprt.elo0 - 0.0).abs() < f64::EPSILON);
        assert!((sprt.elo1 - 5.0).abs() < f64::EPSILON);
        assert!((sprt.alpha - 0.05).abs() < f64::EPSILON);
        assert!((sprt.beta - 0.05).abs() < f64::EPSILON);
        assert_eq!(sprt.model, "logistic");
    }

    #[test]
    fn test_sprt_preserves_explicit_rounds() {
        // When rounds is already set, SPRT should not override it
        let args = make_args(&["-sprt", "elo0=0", "elo1=5", "alpha=0.05", "beta=0.05"]);
        let parser = OptionsParser::new(&args).unwrap();
        // rounds=1 from make_args, SPRT should not overwrite
        assert_eq!(parser.tournament_config().rounds, 1);
    }

    #[test]
    fn test_sprt_unrecognized_key() {
        let args = make_args(&["-sprt", "badkey=1"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    // -----------------------------------------------------------------------
    // Output options
    // -----------------------------------------------------------------------

    #[test]
    fn test_pgnout() {
        let args = make_args(&[
            "-pgnout",
            "file=games.pgn",
            "append=false",
            "nodes=true",
            "seldepth=true",
            "nps=true",
            "hashfull=true",
            "tbhits=true",
            "timeleft=true",
            "latency=true",
            "min=true",
            "pv=true",
            "notation=lan",
        ]);
        let parser = OptionsParser::new(&args).unwrap();
        let pgn = &parser.tournament_config().pgn;
        assert_eq!(pgn.file, "games.pgn");
        assert!(!pgn.append_file);
        assert!(pgn.track_nodes);
        assert!(pgn.track_seldepth);
        assert!(pgn.track_nps);
        assert!(pgn.track_hashfull);
        assert!(pgn.track_tbhits);
        assert!(pgn.track_timeleft);
        assert!(pgn.track_latency);
        assert!(pgn.min);
        assert!(pgn.track_pv);
        assert_eq!(pgn.notation, NotationType::Lan);
    }

    #[test]
    fn test_pgnout_notation_uci() {
        let args = make_args(&["-pgnout", "file=out.pgn", "notation=uci"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().pgn.notation, NotationType::Uci);
    }

    #[test]
    fn test_pgnout_notation_san() {
        let args = make_args(&["-pgnout", "file=out.pgn", "notation=san"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().pgn.notation, NotationType::San);
    }

    #[test]
    fn test_pgnout_notation_invalid() {
        let args = make_args(&["-pgnout", "file=out.pgn", "notation=invalid"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_pgnout_missing_file() {
        let args = make_args(&["-pgnout", "nodes=true"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_pgnout_match_line() {
        let args = make_args(&["-pgnout", "file=out.pgn", "match_line=pattern1"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(
            parser.tournament_config().pgn.additional_lines_rgx,
            vec!["pattern1".to_string()]
        );
    }

    #[test]
    fn test_pgnout_unrecognized_key() {
        let args = make_args(&["-pgnout", "file=out.pgn", "badkey=true"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_epdout() {
        let args = make_args(&["-epdout", "file=games.epd", "append=false"]);
        let parser = OptionsParser::new(&args).unwrap();
        let epd = &parser.tournament_config().epd;
        assert_eq!(epd.file, "games.epd");
        assert!(!epd.append_file);
    }

    #[test]
    fn test_epdout_missing_file() {
        let args = make_args(&["-epdout", "append=true"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_epdout_unrecognized_key() {
        let args = make_args(&["-epdout", "file=out.epd", "badkey=1"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_output_format_cutechess() {
        let args = make_args(&["-output", "format=cutechess"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().output, OutputType::Cutechess);
    }

    #[test]
    fn test_output_format_fastchess() {
        let args = make_args(&["-output", "format=fastchess"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().output, OutputType::Fastchess);
    }

    #[test]
    fn test_output_format_invalid() {
        let args = make_args(&["-output", "format=invalid"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_output_unrecognized_key() {
        let args = make_args(&["-output", "badkey=1"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_report_penta() {
        let args = make_args(&["-report", "penta=false"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(!parser.tournament_config().report_penta);
    }

    #[test]
    fn test_report_penta_true() {
        let args = make_args(&["-report", "penta=true"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().report_penta);
    }

    #[test]
    fn test_report_unrecognized_key() {
        let args = make_args(&["-report", "badkey=true"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_ratinginterval() {
        let args = make_args(&["-ratinginterval", "50"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().ratinginterval, 50);
    }

    #[test]
    fn test_scoreinterval() {
        let args = make_args(&["-scoreinterval", "5"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().scoreinterval, 5);
    }

    #[test]
    fn test_autosaveinterval() {
        let args = make_args(&["-autosaveinterval", "100"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().autosaveinterval, 100);
    }

    // -----------------------------------------------------------------------
    // Logging options
    // -----------------------------------------------------------------------

    #[test]
    fn test_log_all_options() {
        let args = make_args(&[
            "-log",
            "file=test.log",
            "level=trace",
            "append=false",
            "compress=true",
            "realtime=false",
            "engine=true",
        ]);
        let parser = OptionsParser::new(&args).unwrap();
        let log = &parser.tournament_config().log;
        assert_eq!(log.file, "test.log");
        assert_eq!(log.level, LogLevel::Trace);
        assert!(!log.append_file);
        assert!(log.compress);
        assert!(!log.realtime);
        assert!(log.engine_coms);
    }

    #[test]
    fn test_log_level_info() {
        let args = make_args(&["-log", "file=t.log", "level=info"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().log.level, LogLevel::Info);
    }

    #[test]
    fn test_log_level_warn() {
        let args = make_args(&["-log", "file=t.log", "level=warn"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().log.level, LogLevel::Warn);
    }

    #[test]
    fn test_log_level_err() {
        let args = make_args(&["-log", "file=t.log", "level=err"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().log.level, LogLevel::Error);
    }

    #[test]
    fn test_log_level_fatal() {
        let args = make_args(&["-log", "file=t.log", "level=fatal"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().log.level, LogLevel::Fatal);
    }

    #[test]
    fn test_log_level_invalid() {
        let args = make_args(&["-log", "file=t.log", "level=debug"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    #[test]
    fn test_log_unrecognized_key() {
        let args = make_args(&["-log", "file=t.log", "badkey=true"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    // -----------------------------------------------------------------------
    // CRC32 options
    // -----------------------------------------------------------------------

    #[test]
    fn test_crc32_pgn_true() {
        let args = make_args(&["-crc32", "pgn=true"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().pgn.crc);
    }

    #[test]
    fn test_crc32_pgn_false() {
        let args = make_args(&["-crc32", "pgn=false"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(!parser.tournament_config().pgn.crc);
    }

    #[test]
    fn test_crc32_unrecognized_key() {
        let args = make_args(&["-crc32", "badkey=true"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    // -----------------------------------------------------------------------
    // Misc options
    // -----------------------------------------------------------------------

    #[test]
    fn test_show_latency() {
        let args = make_args(&["-show-latency"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().show_latency);
    }

    #[test]
    fn test_use_affinity() {
        let args = make_args(&["-use-affinity"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().affinity);
        assert!(parser.tournament_config().affinity_cpus.is_empty());
    }

    #[test]
    fn test_use_affinity_with_cpus() {
        let args = make_args(&["-use-affinity", "0,1,4-6"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().affinity);
        assert_eq!(
            parser.tournament_config().affinity_cpus,
            vec![0, 1, 4, 5, 6]
        );
    }

    #[test]
    fn test_event() {
        let args = make_args(&["-event", "MyTournament"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().pgn.event_name, "MyTournament");
    }

    #[test]
    fn test_site() {
        let args = make_args(&["-site", "localhost"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert_eq!(parser.tournament_config().pgn.site, "localhost");
    }

    #[test]
    fn test_testenv() {
        let args = make_args(&["-testEnv"]);
        let parser = OptionsParser::new(&args).unwrap();
        assert!(parser.tournament_config().test_env);
    }

    #[test]
    fn test_debug_errors() {
        let args = make_args(&["-debug"]);
        assert!(OptionsParser::new(&args).is_err());
    }

    // -----------------------------------------------------------------------
    // Engine key=value parsing (additional coverage)
    // -----------------------------------------------------------------------

    #[test]
    fn test_parse_engine_st() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "st", "0.5").unwrap();
        assert_eq!(config.limit.tc.fixed_time, 500);
    }

    #[test]
    fn test_parse_engine_timemargin() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "timemargin", "50").unwrap();
        assert_eq!(config.limit.tc.timemargin, 50);
    }

    #[test]
    fn test_parse_engine_timemargin_negative() {
        let mut config = EngineConfiguration::default();
        assert!(parse_engine_key_value(&mut config, "timemargin", "-5").is_err());
    }

    #[test]
    fn test_parse_engine_nodes() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "nodes", "10000").unwrap();
        assert_eq!(config.limit.nodes, 10000);
    }

    #[test]
    fn test_parse_engine_plies() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "plies", "20").unwrap();
        assert_eq!(config.limit.plies, 20);
    }

    #[test]
    fn test_parse_engine_depth() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "depth", "15").unwrap();
        assert_eq!(config.limit.plies, 15);
    }

    #[test]
    fn test_parse_engine_dir() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "dir", "/usr/local/bin").unwrap();
        assert_eq!(config.dir, "/usr/local/bin");
    }

    #[test]
    fn test_parse_engine_args() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "args", "--uci").unwrap();
        assert_eq!(config.args, "--uci");
    }

    #[test]
    fn test_parse_engine_proto_uci() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "proto", "uci").unwrap();
    }

    #[test]
    fn test_parse_engine_proto_unsupported() {
        let mut config = EngineConfiguration::default();
        assert!(parse_engine_key_value(&mut config, "proto", "xboard").is_err());
    }

    #[test]
    fn test_parse_engine_multiple_options() {
        let mut config = EngineConfiguration::default();
        parse_engine_key_value(&mut config, "option.Hash", "128").unwrap();
        parse_engine_key_value(&mut config, "option.Threads", "4").unwrap();
        assert_eq!(config.options.len(), 2);
        assert_eq!(config.options[0], ("Hash".to_string(), "128".to_string()));
        assert_eq!(config.options[1], ("Threads".to_string(), "4".to_string()));
    }

    // -----------------------------------------------------------------------
    // Combined / integration tests
    // -----------------------------------------------------------------------

    #[test]
    fn test_many_options_combined() {
        let args = make_args(&[
            "-concurrency",
            "1",
            "-games",
            "2",
            "-noswap",
            "-recover",
            "-draw",
            "movenumber=30",
            "movecount=8",
            "score=10",
            "-resign",
            "movecount=5",
            "score=500",
            "-maxmoves",
            "150",
            "-srand",
            "12345",
            "-variant",
            "standard",
            "-tournament",
            "roundrobin",
        ]);
        let parser = OptionsParser::new(&args).unwrap();
        let tc = parser.tournament_config();
        assert_eq!(tc.concurrency, 1);
        assert_eq!(tc.games, 2);
        assert!(tc.noswap);
        assert!(tc.recover);
        assert!(tc.draw.enabled);
        assert_eq!(tc.draw.move_number, 30);
        assert!(tc.resign.enabled);
        assert_eq!(tc.resign.score, 500);
        assert!(tc.maxmoves.enabled);
        assert_eq!(tc.maxmoves.move_count, 150);
        assert_eq!(tc.seed, 12345);
    }

    #[test]
    fn test_each_with_engine_options() {
        let args: Vec<String> = vec![
            "fastchess",
            "-engine",
            "cmd=eng1",
            "name=E1",
            "-engine",
            "cmd=eng2",
            "name=E2",
            "-each",
            "tc=10+0.1",
            "option.Hash=128",
            "-rounds",
            "1",
        ]
        .into_iter()
        .map(|s| s.to_string())
        .collect();

        let parser = OptionsParser::new(&args).unwrap();
        for config in parser.engine_configs() {
            assert_eq!(config.limit.tc.time, 10000);
            assert_eq!(config.limit.tc.increment, 100);
            assert_eq!(
                config.options,
                vec![("Hash".to_string(), "128".to_string())]
            );
        }
    }

    // -----------------------------------------------------------------------
    // Time control edge cases
    // -----------------------------------------------------------------------

    #[test]
    fn test_parse_tc_hourglass_rejected() {
        assert!(parse_tc("10hg").is_err());
    }

    #[test]
    fn test_parse_tc_inf() {
        let tc = parse_tc("inf").unwrap();
        assert_eq!(tc.time, 0);
        assert_eq!(tc.increment, 0);
    }
}
