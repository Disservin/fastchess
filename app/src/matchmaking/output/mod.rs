//! Output formatting for tournament results.
//!
//! Ports `matchmaking/output/output.hpp`, `output_fastchess.hpp`,
//! `output_cutechess.hpp`, and `output_factory.hpp` from C++.
//!
//! The C++ code uses virtual dispatch (IOutput → Fastchess / Cutechess).
//! In Rust we use a trait `Output` with two implementations.

use crate::engine::uci_engine::UciEngine;
use crate::matchmaking::elo::{self, EloResult};
use crate::matchmaking::scoreboard::ScoreBoard;
use crate::matchmaking::sprt::Sprt;
use crate::matchmaking::stats::Stats;
use crate::types::engine_config::{EngineConfiguration, GamePair};
use crate::types::enums::OutputType;

/// Information needed for displaying engine metadata in output.
/// This is a lightweight alternative to passing full UciEngine references.
#[derive(Debug, Clone)]
pub struct EngineDisplayInfo {
    pub config: EngineConfiguration,
    pub threads: Option<String>,
    pub hash: Option<String>,
}

impl EngineDisplayInfo {
    /// Create EngineDisplayInfo from a UciEngine reference.
    pub fn from_uci_engine(engine: &UciEngine) -> Self {
        let threads = engine
            .uci_options()
            .get_option("Threads")
            .map(|opt| opt.value_string());
        let hash = engine
            .uci_options()
            .get_option("Hash")
            .map(|opt| opt.value_string());
        Self {
            config: engine.config().clone(),
            threads,
            hash,
        }
    }
}

/// Format a Stats result as a game-result string.
pub fn format_stats(stats: &Stats) -> &'static str {
    if stats.wins > 0 {
        "1-0"
    } else if stats.losses > 0 {
        "0-1"
    } else if stats.draws > 0 {
        "1/2-1/2"
    } else {
        "*"
    }
}

/// Interface for outputting tournament state to the user.
pub trait Output: Send {
    /// Interval output — displayed every N games based on `ratinginterval`.
    fn print_interval(
        &self,
        sprt: &Sprt,
        stats: &Stats,
        first: &str,
        second: &str,
        engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        book: &str,
        scoreboard: &ScoreBoard,
    ) {
        println!("--------------------------------------------------");
        print!(
            "{}",
            self.format_elo(stats, first, second, engines, book, scoreboard)
        );
        print!("{}", self.format_sprt(sprt, stats));
        println!("--------------------------------------------------");
    }

    /// Print current H2H score result.
    fn print_result(&self, stats: &Stats, first: &str, second: &str);

    /// Format Elo statistics as a string.
    fn format_elo(
        &self,
        stats: &Stats,
        first: &str,
        second: &str,
        engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        book: &str,
        scoreboard: &ScoreBoard,
    ) -> String;

    /// Format SPRT statistics as a string.
    fn format_sprt(&self, sprt: &Sprt, stats: &Stats) -> String;

    /// Print game start notification.
    fn start_game(
        &self,
        configs: &GamePair<&EngineConfiguration, &EngineConfiguration>,
        current_game_count: usize,
        max_game_count: usize,
    );

    /// Print game end notification.
    fn end_game(
        &self,
        configs: &GamePair<&EngineConfiguration, &EngineConfiguration>,
        stats: &Stats,
        annotation: &str,
        id: usize,
    );

    /// Print tournament end message.
    fn end_tournament(&self, termination_message: &str);

    /// Get the output type.
    fn output_type(&self) -> OutputType;
}

// ---------------------------------------------------------------------------
// Fastchess output format
// ---------------------------------------------------------------------------

pub struct FastchessOutput {
    report_penta: bool,
}

impl FastchessOutput {
    pub fn new(report_penta: bool) -> Self {
        Self { report_penta }
    }

    fn create_elo(&self, stats: &Stats) -> EloResult {
        if self.report_penta {
            elo::elo_pentanomial(stats)
        } else {
            elo::elo_wdl(stats)
        }
    }

    fn format_penta_stats(stats: &Stats) -> String {
        format!(
            "[{}, {}, {}, {}, {}]",
            stats.penta_ll,
            stats.penta_ld,
            stats.penta_wl + stats.penta_dd,
            stats.penta_wd,
            stats.penta_ww,
        )
    }

    fn format_wldd_ratio(stats: &Stats) -> String {
        format!("WL/DD Ratio: {:.2}", stats.wl_dd_ratio())
    }

    fn format_elo_h2h(
        &self,
        stats: &Stats,
        first: &str,
        second: &str,
        engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        book: &str,
    ) -> String {
        let elo_result = self.create_elo(stats);

        let first_engine = if engines.0.config.name == first {
            engines.0
        } else {
            engines.1
        };
        let second_engine = if engines.0.config.name == second {
            engines.0
        } else {
            engines.1
        };

        let tc = self.format_time_control(&first_engine.config, &second_engine.config);
        let threads = self.format_threads_from_engines(first_engine, second_engine);
        let hash = self.format_hash_from_engines(first_engine, second_engine);
        let bookname = get_short_name(book);

        let matchup = self.format_matchup(first, second, &tc, &threads, &hash, &bookname);
        let elo_str = format!(
            "Elo: {}, nElo: {}",
            elo_result.get_elo(),
            elo_result.get_nelo()
        );
        let draw_ratio = if self.report_penta {
            stats.draw_ratio_penta()
        } else {
            stats.draw_ratio()
        };
        let game_stats = format!(
            "LOS: {}, DrawRatio: {:.2} %{}",
            elo_result.get_los(),
            draw_ratio,
            if self.report_penta {
                format!(", PairsRatio: {:.2}", stats.pairs_ratio())
            } else {
                String::new()
            }
        );
        let game_results = format!(
            "Games: {}, Wins: {}, Losses: {}, Draws: {}, Points: {:.1} ({:.2} %)",
            stats.sum(),
            stats.wins,
            stats.losses,
            stats.draws,
            stats.points(),
            stats.points_ratio(),
        );

        let mut result = format!("{}\n{}\n{}\n{}", matchup, elo_str, game_stats, game_results);

        if self.report_penta {
            result += &format!(
                "\nPtnml(0-2): {}, {}",
                Self::format_penta_stats(stats),
                Self::format_wldd_ratio(stats),
            );
        }

        result + "\n"
    }

    fn get_time(config: &EngineConfiguration) -> String {
        let limit = &config.limit;
        let tc = &limit.tc;

        if tc.time + tc.increment > 0 {
            let moves = if tc.moves > 0 {
                format!("{}/", tc.moves)
            } else {
                String::new()
            };
            let time = tc.time as f64 / 1000.0;
            let increment = if tc.increment > 0 {
                format!("+{:.2}", tc.increment as f64 / 1000.0)
            } else {
                String::new()
            };
            format!("{}{}{}", moves, time, increment)
        } else if tc.fixed_time > 0 {
            let time = tc.fixed_time as f64 / 1000.0;
            format!("{}/move", time)
        } else if limit.plies > 0 {
            format!("{} plies", limit.plies)
        } else if limit.nodes > 0 {
            format!("{} nodes", limit.nodes)
        } else {
            String::new()
        }
    }

    fn get_threads(engine: &EngineDisplayInfo) -> String {
        if let Some(ref threads) = engine.threads {
            format!("{}t", threads)
        } else {
            "NULL".to_string()
        }
    }

    fn get_hash(engine: &EngineDisplayInfo) -> String {
        if let Some(ref hash) = engine.hash {
            format!("{}MB", hash)
        } else {
            "NULL".to_string()
        }
    }

    fn format_matchup(
        &self,
        first: &str,
        second: &str,
        tc: &str,
        threads: &str,
        hash: &str,
        bookname: &str,
    ) -> String {
        let book_part = if bookname.is_empty() {
            String::new()
        } else {
            format!(", {}", bookname)
        };
        format!(
            "Results of {} vs {} ({}, {}, {}{}):",
            first, second, tc, threads, hash, book_part
        )
    }

    fn format_time_control(
        &self,
        first_config: &EngineConfiguration,
        second_config: &EngineConfiguration,
    ) -> String {
        let time_first = Self::get_time(first_config);
        let time_second = Self::get_time(second_config);
        if time_first == time_second {
            time_first
        } else {
            format!("{} - {}", time_first, time_second)
        }
    }

    fn format_threads_from_engines(
        &self,
        first: &EngineDisplayInfo,
        second: &EngineDisplayInfo,
    ) -> String {
        let t1 = Self::get_threads(first);
        let t2 = Self::get_threads(second);
        if t1 == t2 {
            t1
        } else {
            format!("{} - {}", t1, t2)
        }
    }

    fn format_hash_from_engines(
        &self,
        first: &EngineDisplayInfo,
        second: &EngineDisplayInfo,
    ) -> String {
        let h1 = Self::get_hash(first);
        let h2 = Self::get_hash(second);
        if h1 == h2 {
            h1
        } else {
            format!("{} - {}", h1, h2)
        }
    }
}

impl Output for FastchessOutput {
    fn print_interval(
        &self,
        sprt: &Sprt,
        stats: &Stats,
        first: &str,
        second: &str,
        engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        book: &str,
        scoreboard: &ScoreBoard,
    ) {
        println!("--------------------------------------------------");
        print!(
            "{}",
            self.format_elo(stats, first, second, engines, book, scoreboard)
        );
        print!("{}", self.format_sprt(sprt, stats));
        println!("--------------------------------------------------");
    }

    fn print_result(&self, _stats: &Stats, _first: &str, _second: &str) {
        // Fastchess format does not print per-game results
    }

    fn format_elo(
        &self,
        stats: &Stats,
        first: &str,
        second: &str,
        engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        book: &str,
        scoreboard: &ScoreBoard,
    ) -> String {
        let engine_configs = crate::core::config::engine_configs();

        if engine_configs.len() == 2 {
            return self.format_elo_h2h(stats, first, second, engines, book);
        }

        // Multi-engine ranking table
        let mut elos: Vec<(&EngineConfiguration, EloResult, Stats)> = engine_configs
            .iter()
            .map(|ec| {
                let s = scoreboard.get_all_stats(&ec.name);
                let e = self.create_elo(&s);
                (ec, e, s)
            })
            .collect();

        elos.sort_by(|a, b| {
            let a_elo = a.1.diff;
            let b_elo = b.1.diff;
            b_elo
                .partial_cmp(&a_elo)
                .unwrap_or(std::cmp::Ordering::Equal)
        });

        let max_name_len = elos
            .iter()
            .map(|(ec, _, _)| ec.name.len())
            .max()
            .unwrap_or(25)
            .max(25);

        let mut out = format!(
            "{:<4} {:<width$} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>20}\n",
            "Rank",
            "Name",
            "Elo",
            "+/-",
            "nElo",
            "+/-",
            "Games",
            "Score",
            "Draw",
            "Ptnml(0-2)",
            width = max_name_len,
        );

        for (rank, (ec, elo_result, s)) in elos.iter().enumerate() {
            let draw = if self.report_penta {
                s.draw_ratio_penta()
            } else {
                s.draw_ratio()
            };
            let penta = if self.report_penta {
                Self::format_penta_stats(s)
            } else {
                String::new()
            };
            out += &format!(
                "{:>4} {:<width$} {:>10.2} {:>10.2} {:>10.2} {:>10.2} {:>10} {:>9.1}% {:>9.1}% {:>20}\n",
                rank + 1,
                ec.name,
                elo_result.diff,
                elo_result.error,
                elo_result.nelo_diff,
                elo_result.nelo_error,
                s.sum(),
                s.points_ratio(),
                draw,
                penta,
                width = max_name_len,
            );
        }

        out
    }

    fn format_sprt(&self, sprt: &Sprt, stats: &Stats) -> String {
        if sprt.is_enabled() {
            let llr = sprt.get_llr(stats, self.report_penta);
            format!(
                "LLR: {:.2} ({:.1}%) {} {}\n",
                llr,
                sprt.get_fraction(llr) * 100.0,
                sprt.get_bounds(),
                sprt.get_elo()
            )
        } else {
            String::new()
        }
    }

    fn start_game(
        &self,
        configs: &GamePair<&EngineConfiguration, &EngineConfiguration>,
        current_game_count: usize,
        max_game_count: usize,
    ) {
        println!(
            "Started game {} of {} ({} vs {})",
            current_game_count, max_game_count, configs.white.name, configs.black.name
        );
    }

    fn end_game(
        &self,
        configs: &GamePair<&EngineConfiguration, &EngineConfiguration>,
        stats: &Stats,
        annotation: &str,
        id: usize,
    ) {
        println!(
            "Finished game {} ({} vs {}): {} {{{}}}",
            id,
            configs.white.name,
            configs.black.name,
            format_stats(stats),
            annotation
        );
    }

    fn end_tournament(&self, termination_message: &str) {
        if !termination_message.is_empty() {
            println!("{}", termination_message);
        } else {
            println!("Tournament finished");
        }
    }

    fn output_type(&self) -> OutputType {
        OutputType::Fastchess
    }
}

// ---------------------------------------------------------------------------
// Cutechess output format
// ---------------------------------------------------------------------------

pub struct CutechessOutput;

impl Default for CutechessOutput {
    fn default() -> Self {
        Self::new()
    }
}

impl CutechessOutput {
    pub fn new() -> Self {
        Self
    }
}

impl Output for CutechessOutput {
    fn print_interval(
        &self,
        sprt: &Sprt,
        stats: &Stats,
        first: &str,
        second: &str,
        engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        book: &str,
        scoreboard: &ScoreBoard,
    ) {
        print!(
            "{}",
            self.format_elo(stats, first, second, engines, book, scoreboard)
        );
        print!("{}", self.format_sprt(sprt, stats));
    }

    fn print_result(&self, stats: &Stats, first: &str, second: &str) {
        let elo_result = elo::elo_wdl(stats);
        println!(
            "Score of {} vs {}: {} - {} - {}  [{:.3}] {}",
            first,
            second,
            stats.wins,
            stats.losses,
            stats.draws,
            elo_result.score,
            stats.sum()
        );
    }

    fn format_elo(
        &self,
        stats: &Stats,
        _first: &str,
        _second: &str,
        _engines: (&EngineDisplayInfo, &EngineDisplayInfo),
        _book: &str,
        scoreboard: &ScoreBoard,
    ) -> String {
        let engine_configs = crate::core::config::engine_configs();

        if engine_configs.len() == 2 {
            let elo_result = elo::elo_wdl(stats);
            return format!(
                "Elo difference: {}, LOS: {}, DrawRatio: {:.2} %\n",
                elo_result.get_elo(),
                elo_result.get_los(),
                stats.draw_ratio()
            );
        }

        // Multi-engine ranking table
        let mut elos: Vec<(&EngineConfiguration, EloResult, Stats)> = engine_configs
            .iter()
            .map(|ec| {
                let s = scoreboard.get_all_stats(&ec.name);
                let e = elo::elo_wdl(&s);
                (ec, e, s)
            })
            .collect();

        elos.sort_by(|a, b| {
            let a_elo = a.1.diff;
            let b_elo = b.1.diff;
            b_elo
                .partial_cmp(&a_elo)
                .unwrap_or(std::cmp::Ordering::Equal)
        });

        let max_name_len = elos
            .iter()
            .map(|(ec, _, _)| ec.name.len())
            .max()
            .unwrap_or(25)
            .max(25);

        let mut out = format!(
            "{:<4} {:<width$} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}\n",
            "Rank",
            "Name",
            "Elo",
            "+/-",
            "nElo",
            "+/-",
            "Games",
            "Score",
            "Draw",
            width = max_name_len,
        );

        for (rank, (ec, elo_result, s)) in elos.iter().enumerate() {
            out += &format!(
                "{:>4} {:<width$} {:>10.2} {:>10.2} {:>10.2} {:>10.2} {:>10} {:>9.1}% {:>9.1}%\n",
                rank + 1,
                ec.name,
                elo_result.diff,
                elo_result.error,
                elo_result.nelo_diff,
                elo_result.nelo_error,
                s.sum(),
                s.points_ratio(),
                s.draw_ratio(),
                width = max_name_len,
            );
        }

        out
    }

    fn format_sprt(&self, sprt: &Sprt, stats: &Stats) -> String {
        if sprt.is_enabled() {
            let lower_bound = sprt.lower_bound();
            let upper_bound = sprt.upper_bound();
            let llr = sprt.get_llr(stats, false);
            let percentage = if llr < 0.0 {
                llr / lower_bound * 100.0
            } else {
                llr / upper_bound * 100.0
            };

            let result_str = if llr >= upper_bound {
                " - H1 was accepted"
            } else if llr < lower_bound {
                " - H0 was accepted"
            } else {
                ""
            };

            format!(
                "SPRT: llr {:.2} ({:.1}%), lbound {:.2}, ubound {:.2}{}\n",
                llr, percentage, lower_bound, upper_bound, result_str
            )
        } else {
            String::new()
        }
    }

    fn start_game(
        &self,
        configs: &GamePair<&EngineConfiguration, &EngineConfiguration>,
        current_game_count: usize,
        max_game_count: usize,
    ) {
        println!(
            "Started game {} of {} ({} vs {})",
            current_game_count, max_game_count, configs.white.name, configs.black.name
        );
    }

    fn end_game(
        &self,
        configs: &GamePair<&EngineConfiguration, &EngineConfiguration>,
        stats: &Stats,
        annotation: &str,
        id: usize,
    ) {
        println!(
            "Finished game {} ({} vs {}): {} {{{}}}",
            id,
            configs.white.name,
            configs.black.name,
            format_stats(stats),
            annotation
        );
    }

    fn end_tournament(&self, _termination_message: &str) {
        println!("Tournament finished");
    }

    fn output_type(&self) -> OutputType {
        OutputType::Cutechess
    }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

/// Create an output formatter for the given type.
pub fn create_output(output_type: OutputType, report_penta: bool) -> Box<dyn Output + Send> {
    match output_type {
        OutputType::Fastchess | OutputType::None => Box::new(FastchessOutput::new(report_penta)),
        OutputType::Cutechess => Box::new(CutechessOutput::new()),
    }
}

/// Parse an output type from a string.
pub fn parse_output_type(s: &str) -> OutputType {
    match s {
        "cutechess" => OutputType::Cutechess,
        _ => OutputType::Fastchess,
    }
}

fn get_short_name(name: &str) -> String {
    if let Some(pos) = name.rfind(['/', '\\']) {
        name[pos + 1..].to_string()
    } else {
        name.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format_stats() {
        assert_eq!(format_stats(&Stats::new(1, 0, 0)), "1-0");
        assert_eq!(format_stats(&Stats::new(0, 1, 0)), "0-1");
        assert_eq!(format_stats(&Stats::new(0, 0, 1)), "1/2-1/2");
        assert_eq!(format_stats(&Stats::default()), "*");
    }

    #[test]
    fn test_get_short_name() {
        assert_eq!(get_short_name("/path/to/book.epd"), "book.epd");
        assert_eq!(get_short_name("book.epd"), "book.epd");
        assert_eq!(get_short_name("C:\\path\\to\\book.epd"), "book.epd");
    }

    #[test]
    fn test_parse_output_type() {
        assert_eq!(parse_output_type("fastchess"), OutputType::Fastchess);
        assert_eq!(parse_output_type("cutechess"), OutputType::Cutechess);
        assert_eq!(parse_output_type("unknown"), OutputType::Fastchess);
    }

    #[test]
    fn test_penta_stats_format() {
        let stats = Stats {
            penta_ll: 1,
            penta_ld: 2,
            penta_wl: 3,
            penta_dd: 4,
            penta_wd: 5,
            penta_ww: 6,
            ..Default::default()
        };
        let formatted = FastchessOutput::format_penta_stats(&stats);
        assert_eq!(formatted, "[1, 2, 7, 5, 6]");
    }
}
