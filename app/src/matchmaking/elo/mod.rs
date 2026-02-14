//! Elo rating computation from match statistics.
//!
//! Ports `matchmaking/elo/elo.hpp`, `elo_wdl.hpp/cpp`, and
//! `elo_pentanomial.hpp/cpp` from C++.
//!
//! The C++ code uses virtual dispatch (EloBase → EloWDL / EloPentanomial).
//! In Rust we use an enum with shared computation in the base fields.

use super::stats::Stats;

/// 95% confidence interval z-score.
const CI95_ZSCORE: f64 = 1.959963984540054;

/// Computed Elo statistics (shared fields for both WDL and Pentanomial).
#[derive(Debug, Clone)]
pub struct EloResult {
    pub diff: f64,
    pub error: f64,
    pub nelo_diff: f64,
    pub nelo_error: f64,
    pub score: f64,
    pub los: f64,
}

impl EloResult {
    /// Format the Elo difference as "diff +/- error".
    pub fn get_elo(&self) -> String {
        format!("{:.2} +/- {:.2}", self.diff, self.error)
    }

    /// Format the normalized Elo as "nelo_diff +/- nelo_error".
    pub fn get_nelo(&self) -> String {
        format!("{:.2} +/- {:.2}", self.nelo_diff, self.nelo_error)
    }

    /// Format LOS as a percentage string.
    pub fn get_los(&self) -> String {
        format!("{:.2} %", self.los * 100.0)
    }
}

/// Convert a score (0..1) to an Elo difference.
fn score_to_elo_diff(score: f64) -> f64 {
    -400.0 * (1.0 / score - 1.0).log10()
}

// --- EloWDL ---

/// Compute Elo statistics from Win/Draw/Loss counts.
pub fn elo_wdl(stats: &Stats) -> EloResult {
    let games = stats.sum() as f64;
    let score = calc_score_wdl(stats, games);
    let variance = calc_variance_wdl(stats, games, score);
    let variance_per_game = variance / games;

    let score_upper = score + CI95_ZSCORE * variance_per_game.sqrt();
    let score_lower = score - CI95_ZSCORE * variance_per_game.sqrt();

    let diff = score_to_elo_diff(score);
    let error = (score_to_elo_diff(score_upper) - score_to_elo_diff(score_lower)) / 2.0;
    let nelo_diff = score_to_nelo_diff_wdl(score, variance);
    let nelo_error = (score_to_nelo_diff_wdl(score_upper, variance)
        - score_to_nelo_diff_wdl(score_lower, variance))
        / 2.0;

    let los = (1.0 - libm::erf(-(score - 0.5) / (2.0 * variance_per_game).sqrt())) / 2.0;

    EloResult {
        diff,
        error,
        nelo_diff,
        nelo_error,
        score,
        los,
    }
}

fn calc_score_wdl(stats: &Stats, games: f64) -> f64 {
    let w = stats.wins as f64 / games;
    let d = stats.draws as f64 / games;
    w + 0.5 * d
}

fn calc_variance_wdl(stats: &Stats, games: f64, score: f64) -> f64 {
    let w = stats.wins as f64 / games;
    let d = stats.draws as f64 / games;
    let l = stats.losses as f64 / games;
    let w_dev = w * (1.0 - score).powi(2);
    let d_dev = d * (0.5 - score).powi(2);
    let l_dev = l * (0.0 - score).powi(2);
    w_dev + d_dev + l_dev
}

fn score_to_nelo_diff_wdl(score: f64, variance: f64) -> f64 {
    (score - 0.5) / variance.sqrt() * (800.0 / 10.0_f64.ln())
}

// --- EloPentanomial ---

/// Compute Elo statistics from pentanomial pair counts.
pub fn elo_pentanomial(stats: &Stats) -> EloResult {
    let pairs = stats.total_pairs() as f64;
    let score = calc_score_penta(stats, pairs);
    let variance = calc_variance_penta(stats, pairs, score);
    let variance_per_pair = variance / pairs;

    let score_upper = score + CI95_ZSCORE * variance_per_pair.sqrt();
    let score_lower = score - CI95_ZSCORE * variance_per_pair.sqrt();

    let diff = score_to_elo_diff(score);
    let error = (score_to_elo_diff(score_upper) - score_to_elo_diff(score_lower)) / 2.0;
    let nelo_diff = score_to_nelo_diff_penta(score, variance);
    let nelo_error = (score_to_nelo_diff_penta(score_upper, variance)
        - score_to_nelo_diff_penta(score_lower, variance))
        / 2.0;

    let los = (1.0 - libm::erf(-(score - 0.5) / (2.0 * variance_per_pair).sqrt())) / 2.0;

    EloResult {
        diff,
        error,
        nelo_diff,
        nelo_error,
        score,
        los,
    }
}

fn calc_score_penta(stats: &Stats, pairs: f64) -> f64 {
    let ww = stats.penta_ww as f64 / pairs;
    let wd = stats.penta_wd as f64 / pairs;
    let wl = stats.penta_wl as f64 / pairs;
    let dd = stats.penta_dd as f64 / pairs;
    let ld = stats.penta_ld as f64 / pairs;
    ww + 0.75 * wd + 0.5 * (wl + dd) + 0.25 * ld
}

fn calc_variance_penta(stats: &Stats, pairs: f64, score: f64) -> f64 {
    let ww = stats.penta_ww as f64 / pairs;
    let wd = stats.penta_wd as f64 / pairs;
    let wl = stats.penta_wl as f64 / pairs;
    let dd = stats.penta_dd as f64 / pairs;
    let ld = stats.penta_ld as f64 / pairs;
    let ll = stats.penta_ll as f64 / pairs;

    let ww_dev = ww * (1.0 - score).powi(2);
    let wd_dev = wd * (0.75 - score).powi(2);
    let wldd_dev = (wl + dd) * (0.5 - score).powi(2);
    let ld_dev = ld * (0.25 - score).powi(2);
    let ll_dev = ll * (0.0 - score).powi(2);

    ww_dev + wd_dev + wldd_dev + ld_dev + ll_dev
}

fn score_to_nelo_diff_penta(score: f64, variance: f64) -> f64 {
    (score - 0.5) / (2.0 * variance).sqrt() * (800.0 / 10.0_f64.ln())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_approx_eq(actual: f64, expected: f64, epsilon: f64, msg: &str) {
        let diff = (actual - expected).abs();
        assert!(
            diff < epsilon,
            "{}: expected {:.2}, got {:.2}, diff {:.2} exceeds epsilon {:.2}",
            msg,
            expected,
            actual,
            diff,
            epsilon
        );
    }

    // Tests ported from C++ elo_test.cpp with exact expected values
    #[test]
    fn test_elo_wdl_1() {
        // C++: Stats(76, 89, 123)
        let stats = Stats::new(76, 89, 123);
        let result = elo_wdl(&stats);
        // C++: nEloDiff = -20.76, nEloError = 40.13, los = "15.53 %", score = 0.477
        assert_approx_eq(result.nelo_diff, -20.76, 0.01, "nEloDiff");
        assert_approx_eq(result.nelo_error, 40.13, 0.01, "nEloError");
        assert_eq!(result.get_los(), "15.53 %");
        assert_approx_eq(result.score, 0.477, 0.01, "score");
    }

    #[test]
    fn test_elo_wdl_2() {
        // C++: Stats(136, 96, 111)
        let stats = Stats::new(136, 96, 111);
        let result = elo_wdl(&stats);
        // C++: nEloDiff = 49.77, nEloError = 36.77, diff = 40.70, error = 30.43, los = "99.60 %", score = 0.558
        assert_approx_eq(result.nelo_diff, 49.77, 0.01, "nEloDiff");
        assert_approx_eq(result.nelo_error, 36.77, 0.01, "nEloError");
        assert_approx_eq(result.diff, 40.70, 0.01, "diff");
        assert_approx_eq(result.error, 30.43, 0.01, "error");
        assert_eq!(result.get_los(), "99.60 %");
        assert_approx_eq(result.score, 0.558, 0.01, "score");
    }

    #[test]
    fn test_elo_wdl_3() {
        // C++: Stats(34, 356, 0)
        let stats = Stats::new(34, 356, 0);
        let result = elo_wdl(&stats);
        // C++: nEloDiff = -508.44, nEloError = 34.48, score = 0.087
        assert_approx_eq(result.nelo_diff, -508.44, 0.01, "nEloDiff");
        assert_approx_eq(result.nelo_error, 34.48, 0.01, "nEloError");
        assert_approx_eq(result.score, 0.087, 0.01, "score");
    }

    #[test]
    fn test_elo_pentanomial_1() {
        // C++: Stats(34, 54, 31, 32, 64, 75) - WW, WD, WL, DD, LD, LL
        let stats = Stats::from_pentanomial(34, 54, 31, 32, 64, 75);
        let result = elo_pentanomial(&stats);
        // C++: nEloDiff = 57.94, nEloError = 28.28, diff = 55.58, error = 27.65, los = "100.00 %", score = 0.579
        assert_approx_eq(result.nelo_diff, 57.94, 0.01, "nEloDiff");
        assert_approx_eq(result.nelo_error, 28.28, 0.01, "nEloError");
        assert_approx_eq(result.diff, 55.58, 0.01, "diff");
        assert_approx_eq(result.error, 27.65, 0.01, "error");
        assert_eq!(result.get_los(), "100.00 %");
        assert_approx_eq(result.score, 0.579, 0.01, "score");
    }

    #[test]
    fn test_elo_pentanomial_2() {
        // C++: Stats(332, 433, 457, 41, 333, 334)
        let stats = Stats::from_pentanomial(332, 433, 457, 41, 333, 334);
        let result = elo_pentanomial(&stats);
        // C++: nEloDiff = -9.17, nEloError = 10.96, diff = -8.64, error = 10.33, los = "5.05 %", score = 0.488
        assert_approx_eq(result.nelo_diff, -9.17, 0.01, "nEloDiff");
        assert_approx_eq(result.nelo_error, 10.96, 0.01, "nEloError");
        assert_approx_eq(result.diff, -8.64, 0.01, "diff");
        assert_approx_eq(result.error, 10.33, 0.01, "error");
        assert_eq!(result.get_los(), "5.05 %");
        assert_approx_eq(result.score, 0.488, 0.01, "score");
    }

    #[test]
    fn test_elo_pentanomial_3() {
        // C++: Stats(7895, 8757, 5485, 200, 568, 9999)
        let stats = Stats::from_pentanomial(7895, 8757, 5485, 200, 568, 9999);
        let result = elo_pentanomial(&stats);
        // C++: nEloDiff = -19.01, nEloError = 2.65, diff = -21.04, error = 2.95, los = "0.00 %", score = 0.470
        assert_approx_eq(result.nelo_diff, -19.01, 0.01, "nEloDiff");
        assert_approx_eq(result.nelo_error, 2.65, 0.01, "nEloError");
        assert_approx_eq(result.diff, -21.04, 0.01, "diff");
        assert_approx_eq(result.error, 2.95, 0.01, "error");
        assert_eq!(result.get_los(), "0.00 %");
        assert_approx_eq(result.score, 0.470, 0.01, "score");
    }

    #[test]
    fn test_elo_wdl_balanced() {
        let stats = Stats::new(100, 100, 200);
        let result = elo_wdl(&stats);
        // Balanced results should give ~0 elo diff
        assert!(
            result.diff.abs() < 5.0,
            "diff should be near 0, got {}",
            result.diff
        );
        assert!(result.score > 0.49 && result.score < 0.51);
    }

    #[test]
    fn test_elo_wdl_winning() {
        let stats = Stats::new(200, 50, 100);
        let result = elo_wdl(&stats);
        assert!(result.diff > 0.0, "winner should have positive elo");
        assert!(result.nelo_diff > 0.0);
        assert!(result.los > 0.5);
    }

    #[test]
    fn test_elo_wdl_losing() {
        let stats = Stats::new(50, 200, 100);
        let result = elo_wdl(&stats);
        assert!(result.diff < 0.0, "loser should have negative elo");
    }

    #[test]
    fn test_elo_pentanomial_balanced() {
        let stats = Stats::from_pentanomial(10, 30, 20, 100, 30, 10);
        let result = elo_pentanomial(&stats);
        assert!(
            result.diff.abs() < 5.0,
            "balanced penta should give ~0 elo, got {}",
            result.diff
        );
    }

    #[test]
    fn test_elo_pentanomial_winning() {
        let stats = Stats::from_pentanomial(2, 10, 20, 50, 100, 50);
        let result = elo_pentanomial(&stats);
        assert!(result.diff > 0.0);
        assert!(result.nelo_diff > 0.0);
    }

    #[test]
    fn test_format_strings() {
        let stats = Stats::new(150, 100, 200);
        let result = elo_wdl(&stats);
        let elo_str = result.get_elo();
        assert!(elo_str.contains("+/-"));
        let nelo_str = result.get_nelo();
        assert!(nelo_str.contains("+/-"));
        let los_str = result.get_los();
        assert!(los_str.contains("%"));
    }

    #[test]
    fn test_score_to_elo_diff_symmetry() {
        // score=0.5 should give elo=0
        let elo = score_to_elo_diff(0.5);
        assert!(elo.abs() < 1e-10);

        // score>0.5 → positive elo
        assert!(score_to_elo_diff(0.6) > 0.0);
        // score<0.5 → negative elo
        assert!(score_to_elo_diff(0.4) < 0.0);
    }
}
