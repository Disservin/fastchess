use std::io::{self, Write};

use crate::core::config;
use crate::engine::UciEngine;
use crate::types::engine_config::EngineConfiguration;

use colored::Colorize;

fn is_valid_info_line(info_line: &str) -> bool {
    let mut tokens = info_line.split_whitespace();
    let Some(first) = tokens.next() else {
        eprintln!("\r\nInvalid info line format: {}", info_line);
        return false;
    };
    if first != "info" {
        eprintln!("\r\nInvalid info line format: {}", info_line);
        return false;
    }

    let mut iter = tokens.peekable();
    while let Some(token) = iter.next() {
        if token == "time" || token == "nps" || token == "score" {
            let Some(value) = iter.next() else {
                eprintln!("\r\nNo value after token: {}", token);
                return false;
            };
            if value.contains('.') {
                eprintln!("\r\nTime/NPS/Score value is not an integer: {}", value);
                return false;
            }
        }
    }

    true
}

fn contains_token(haystack: &str, needle: &str) -> bool {
    haystack.split_whitespace().any(|t| t == needle)
}

fn print_engine_output(engine: &UciEngine) {
    eprintln!("{}", "Your engine's stdout output was:".yellow().bold());
    for line in engine.get_stdout_lines() {
        eprintln!(" {}", line.line);
    }

    let stderr = engine.get_stderr_lines();
    if !stderr.is_empty() {
        eprintln!("{}", "Your engine's stderr output was:".yellow().bold());
        for line in stderr {
            eprintln!(" {}", line.line);
        }
    }
}

pub fn run_compliance_test(args: &[String]) -> Result<bool, String> {
    if args.len() < 3 {
        return Err("Usage: --compliance <engine_cmd> [engine_args]".to_string());
    }

    config::set_tournament_config(Default::default());

    let mut config = EngineConfiguration::default();
    config.cmd = args[2].clone();
    config.args = if args.len() > 3 {
        args[3].clone()
    } else {
        String::new()
    };

    let mut uci_engine = UciEngine::new(&config, false).map_err(|e| e.to_string())?;

    let mut step = 0;
    let mut execute_step = |description: &str, action: &dyn Fn(&mut UciEngine) -> bool| -> bool {
        step += 1;
        print!("Step {}: {}...", step, description);
        let _ = io::stdout().flush();

        if !action(&mut uci_engine) {
            eprintln!(
                "\r {} {}",
                format!("Failed Step {}:", step).red().bold(),
                description
            );
            print_engine_output(&uci_engine);
            return false;
        }

        println!(
            "\r {} {}",
            format!("Passed Step {}:", step).green().bold(),
            description
        );

        true
    };

    let steps: Vec<(&str, Box<dyn Fn(&mut UciEngine) -> bool>)> = vec![
        (
            "Start the engine",
            Box::new(|engine| engine.start(None).is_ok()),
        ),
        (
            "Check if engine is ready",
            Box::new(|engine| engine.isready(None).is_ok()),
        ),
        (
            "Check id name",
            Box::new(|engine| engine.id_name().is_some()),
        ),
        (
            "Check id author",
            Box::new(|engine| engine.id_author().is_some()),
        ),
        ("Send ucinewgame", Box::new(|engine| engine.ucinewgame())),
        (
            "Set position to startpos",
            Box::new(|engine| engine.write_engine("position startpos")),
        ),
        (
            "Check if engine is ready after startpos",
            Box::new(|engine| engine.isready(None).is_ok()),
        ),
        (
            "Set position to fen",
            Box::new(|engine| {
                engine.write_engine(
                    "position fen 3r2k1/p5n1/1cq1p2p/2p3p1/2P1P1n1/1P1P2pP/PN1Q2K1/5R2 w - - 0 27",
                )
            }),
        ),
        (
            "Check if engine is ready after fen",
            Box::new(|engine| engine.isready(None).is_ok()),
        ),
        (
            "Send go wtime 100",
            Box::new(|engine| engine.write_engine("go wtime 100")),
        ),
        (
            "Read bestmove",
            Box::new(|engine| engine.read_engine("bestm2ove", None).is_ok()),
        ),
        (
            "Check if engine prints an info line",
            Box::new(|engine| !engine.last_info_line().is_empty()),
        ),
        (
            "Verify info line format is valid",
            Box::new(|engine| is_valid_info_line(engine.last_info_line())),
        ),
        (
            "Verify info line contains score",
            Box::new(|engine| contains_token(engine.last_info_line(), "score")),
        ),
        (
            "Set position to black to move",
            Box::new(|engine| {
                engine.write_engine(
                    "position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
                )
            }),
        ),
        (
            "Send go btime 100",
            Box::new(|engine| engine.write_engine("go btime 100")),
        ),
        (
            "Read bestmove after go btime 100",
            Box::new(|engine| engine.read_engine("bestmove", None).is_ok()),
        ),
        (
            "Check if engine prints an info line after go btime 100",
            Box::new(|engine| !engine.last_info_line().is_empty()),
        ),
        (
            "Verify info line format is valid after go btime 100",
            Box::new(|engine| is_valid_info_line(engine.last_info_line())),
        ),
        (
            "Check if engine prints an info line with the score after go btime 100",
            Box::new(|engine| contains_token(engine.last_info_line(), "score")),
        ),
        (
            "Send go wtime 100 winc 100 btime 100 binc 100",
            Box::new(|engine| engine.write_engine("go wtime 100 winc 100 btime 100 binc 100")),
        ),
        (
            "Read bestmove after go wtime 100 winc 100 btime 100 binc 100",
            Box::new(|engine| engine.read_engine("bestmove", None).is_ok()),
        ),
        (
            "Check if engine prints an info line after go wtime 100 winc 100",
            Box::new(|engine| !engine.last_info_line().is_empty()),
        ),
        (
            "Verify info line format is valid after go wtime 100 winc 100",
            Box::new(|engine| is_valid_info_line(engine.last_info_line())),
        ),
        (
            "Check if engine prints an info line with the score after go wtime 100 winc 100",
            Box::new(|engine| contains_token(engine.last_info_line(), "score")),
        ),
        (
            "Send go btime 100 binc 100 wtime 100 winc 100",
            Box::new(|engine| engine.write_engine("go btime 100 binc 100 wtime 100 winc 100")),
        ),
        (
            "Read bestmove after go btime 100 binc 100 wtime 100 winc 100",
            Box::new(|engine| engine.read_engine("bestmove", None).is_ok()),
        ),
        (
            "Check if engine prints an info line after go btime 100 binc 100",
            Box::new(|engine| !engine.last_info_line().is_empty()),
        ),
        (
            "Verify info line format is valid after go btime 100 binc 100",
            Box::new(|engine| is_valid_info_line(engine.last_info_line())),
        ),
        (
            "Check if engine prints an info line with the score after go btime 100 binc 100",
            Box::new(|engine| contains_token(engine.last_info_line(), "score")),
        ),
        (
            "Check if engine prints an info line after go btime 100 binc 100",
            Box::new(|engine| contains_token(engine.last_info_line(), "score")),
        ),
        ("Send ucinewgame", Box::new(|engine| engine.ucinewgame())),
        (
            "Set position to startpos",
            Box::new(|engine| engine.write_engine("position startpos")),
        ),
        (
            "Send go wtime 100",
            Box::new(|engine| engine.write_engine("go wtime 100 btime 100")),
        ),
        (
            "Read bestmove after go wtime 100 btime 100",
            Box::new(|engine| {
                engine.read_engine("bestmove", None).is_ok() && engine.bestmove().is_some()
            }),
        ),
        (
            "Verify info line format is valid after go wtime 100 btime 100",
            Box::new(|engine| is_valid_info_line(engine.last_info_line())),
        ),
        (
            "Set position to startpos moves e2e4 e7e5",
            Box::new(|engine| engine.write_engine("position startpos moves e2e4 e7e5")),
        ),
        (
            "Send go wtime 100 btime 100",
            Box::new(|engine| engine.write_engine("go wtime 100 btime 100")),
        ),
        (
            "Read bestmove after position startpos moves e2e4 e7e5",
            Box::new(|engine| {
                engine.read_engine("bestmove", None).is_ok() && engine.bestmove().is_some()
            }),
        ),
        (
            "Verify info line format is valid after position startpos moves e2e4 e7e5",
            Box::new(|engine| is_valid_info_line(engine.last_info_line())),
        ),
    ];

    for (description, action) in steps {
        if !execute_step(description, &*action) {
            return Ok(false);
        }
    }

    println!("Engine passed all compliance checks.");
    Ok(true)
}
