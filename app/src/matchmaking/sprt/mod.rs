//! Sequential Probability Ratio Test (SPRT) for engine match hypothesis testing.
//!
//! Ports `matchmaking/sprt/sprt.hpp` and `sprt.cpp` from C++.

use super::stats::Stats;

/// Result of an SPRT test.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SprtResult {
    /// Null hypothesis accepted (elo0 is more likely).
    H0,
    /// Alternative hypothesis accepted (elo1 is more likely).
    H1,
    /// Not enough data yet.
    Continue,
}

/// SPRT model type.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SprtModel {
    Normalized,
    Bayesian,
    Logistic,
}

impl SprtModel {
    pub fn from_str(s: &str) -> Result<Self, String> {
        match s {
            "normalized" => Ok(Self::Normalized),
            "bayesian" => Ok(Self::Bayesian),
            "logistic" => Ok(Self::Logistic),
            _ => Err(format!("Invalid SPRT model: {}", s)),
        }
    }

    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Normalized => "normalized",
            Self::Bayesian => "bayesian",
            Self::Logistic => "logistic",
        }
    }
}

/// Sequential Probability Ratio Test.
///
/// Tests H0: elo = elo0 vs H1: elo = elo1 using the log-likelihood ratio.
#[derive(Debug, Clone)]
pub struct Sprt {
    lower: f64,
    upper: f64,
    elo0: f64,
    elo1: f64,
    enabled: bool,
    model: SprtModel,
}

impl Default for Sprt {
    fn default() -> Self {
        Self {
            lower: 0.0,
            upper: 0.0,
            elo0: 0.0,
            elo1: 0.0,
            enabled: false,
            model: SprtModel::Normalized,
        }
    }
}

impl Sprt {
    pub fn new(
        alpha: f64,
        beta: f64,
        elo0: f64,
        elo1: f64,
        model: SprtModel,
        enabled: bool,
    ) -> Self {
        let mut sprt = Self {
            enabled,
            elo0,
            elo1,
            model,
            lower: 0.0,
            upper: 0.0,
        };

        if enabled {
            sprt.lower = (beta / (1.0 - alpha)).ln();
            sprt.upper = ((1.0 - beta) / alpha).ln();
        }

        sprt
    }

    pub fn is_enabled(&self) -> bool {
        self.enabled
    }

    /// Compute the log-likelihood ratio from stats.
    pub fn get_llr(&self, stats: &Stats, penta: bool) -> f64 {
        if penta {
            self.get_llr_penta(
                stats.penta_ww,
                stats.penta_wd,
                stats.penta_wl,
                stats.penta_dd,
                stats.penta_ld,
                stats.penta_ll,
            )
        } else {
            self.get_llr_wdl(stats.wins, stats.draws, stats.losses)
        }
    }

    /// Fraction of the way to a bound (positive = towards H1, negative = towards H0).
    pub fn get_fraction(&self, llr: f64) -> f64 {
        if llr >= 0.0 {
            llr / self.upper
        } else {
            -llr / self.lower
        }
    }

    /// Determine the SPRT result given a log-likelihood ratio.
    pub fn get_result(&self, llr: f64) -> SprtResult {
        if !self.enabled {
            return SprtResult::Continue;
        }
        if llr >= self.upper {
            SprtResult::H1
        } else if llr <= self.lower {
            SprtResult::H0
        } else {
            SprtResult::Continue
        }
    }

    /// Format the SPRT bounds as a string.
    pub fn get_bounds(&self) -> String {
        format!("({:.2}, {:.2})", self.lower, self.upper)
    }

    /// Format the elo range as a string.
    pub fn get_elo(&self) -> String {
        format!("[{:.2}, {:.2}]", self.elo0, self.elo1)
    }

    pub fn lower_bound(&self) -> f64 {
        self.lower
    }

    pub fn upper_bound(&self) -> f64 {
        self.upper
    }

    /// Validate SPRT parameters.
    pub fn validate(
        alpha: f64,
        beta: f64,
        elo0: f64,
        elo1: f64,
        model: &str,
        report_penta: &mut bool,
    ) -> Result<(), String> {
        if elo0 >= elo1 {
            return Err("SPRT: elo0 must be less than elo1".to_string());
        }
        if alpha <= 0.0 || alpha >= 1.0 {
            return Err("SPRT: alpha must be between 0 and 1 (exclusive)".to_string());
        }
        if beta <= 0.0 || beta >= 1.0 {
            return Err("SPRT: beta must be between 0 and 1 (exclusive)".to_string());
        }
        if alpha + beta >= 1.0 {
            return Err("SPRT: sum of alpha and beta must be less than 1".to_string());
        }
        if model != "normalized" && model != "bayesian" && model != "logistic" {
            return Err(format!("SPRT: invalid model '{}'", model));
        }
        if model == "bayesian" && *report_penta {
            eprintln!("Warning; Bayesian SPRT model not available with pentanomial statistics. Disabling pentanomial reports...");
            *report_penta = false;
        }
        Ok(())
    }

    // --- Static conversion helpers ---

    /// Convert logistic elo to expected score.
    pub fn lelo_to_score(lelo: f64) -> f64 {
        1.0 / (1.0 + 10.0_f64.powf(-lelo / 400.0))
    }

    /// Convert BayesElo + drawelo to expected score.
    pub fn bayeselo_to_score(bayeselo: f64, drawelo: f64) -> f64 {
        let pwin = 1.0 / (1.0 + 10.0_f64.powf((-bayeselo + drawelo) / 400.0));
        let ploss = 1.0 / (1.0 + 10.0_f64.powf((bayeselo + drawelo) / 400.0));
        let pdraw = 1.0 - pwin - ploss;
        pwin + 0.5 * pdraw
    }

    /// Convert normalized elo to expected score (WDL model).
    pub fn nelo_to_score_wdl(nelo: f64, variance: f64) -> f64 {
        nelo * variance.sqrt() / (800.0 / 10.0_f64.ln()) + 0.5
    }

    /// Convert normalized elo to expected score (pentanomial model).
    pub fn nelo_to_score_penta(nelo: f64, variance: f64) -> f64 {
        nelo * (2.0 * variance).sqrt() / (800.0 / 10.0_f64.ln()) + 0.5
    }

    // --- Private LLR computation ---

    fn get_llr_wdl(&self, win: i32, draw: i32, loss: i32) -> f64 {
        if !self.enabled {
            return 0.0;
        }

        let l = regularize(loss);
        let d = regularize(draw);
        let w = regularize(win);
        let total = l + d + w;
        let probs = [l / total, d / total, w / total];
        let scores = [0.0, 0.5, 1.0];

        match self.model {
            SprtModel::Normalized => {
                let t0 = self.elo0 / (800.0 / 10.0_f64.ln());
                let t1 = self.elo1 / (800.0 / 10.0_f64.ln());
                self.get_llr_normalized(total, &scores, &probs, t0, t1)
            }
            SprtModel::Bayesian => {
                if win == 0 || loss == 0 {
                    return 0.0;
                }
                let l_prob = probs[0];
                let w_prob = probs[2];
                let drawelo = 200.0 * ((1.0 - l_prob) / l_prob * (1.0 - w_prob) / w_prob).log10();
                let score0 = Self::bayeselo_to_score(self.elo0, drawelo);
                let score1 = Self::bayeselo_to_score(self.elo1, drawelo);
                self.get_llr_logistic(total, &scores, &probs, score0, score1)
            }
            SprtModel::Logistic => {
                let score0 = Self::lelo_to_score(self.elo0);
                let score1 = Self::lelo_to_score(self.elo1);
                self.get_llr_logistic(total, &scores, &probs, score0, score1)
            }
        }
    }

    fn get_llr_penta(
        &self,
        penta_ww: i32,
        penta_wd: i32,
        penta_wl: i32,
        penta_dd: i32,
        penta_ld: i32,
        penta_ll: i32,
    ) -> f64 {
        if !self.enabled {
            return 0.0;
        }

        let ll = regularize(penta_ll);
        let ld = regularize(penta_ld);
        let wl_dd = regularize(penta_dd + penta_wl);
        let wd = regularize(penta_wd);
        let ww = regularize(penta_ww);
        let total = ww + wd + wl_dd + ld + ll;
        let probs = [
            ll / total,
            ld / total,
            wl_dd / total,
            wd / total,
            ww / total,
        ];
        let scores = [0.0, 0.25, 0.5, 0.75, 1.0];

        match self.model {
            SprtModel::Normalized => {
                let t0 = 2.0_f64.sqrt() * self.elo0 / (800.0 / 10.0_f64.ln());
                let t1 = 2.0_f64.sqrt() * self.elo1 / (800.0 / 10.0_f64.ln());
                self.get_llr_normalized(total, &scores, &probs, t0, t1)
            }
            _ => {
                // Bayesian not supported for pentanomial; use logistic
                let score0 = Self::lelo_to_score(self.elo0);
                let score1 = Self::lelo_to_score(self.elo1);
                self.get_llr_logistic(total, &scores, &probs, score0, score1)
            }
        }
    }

    /// Logistic LLR computation (Proposition 1.1 from Van den Bergh).
    fn get_llr_logistic(&self, total: f64, scores: &[f64], probs: &[f64], s0: f64, s1: f64) -> f64 {
        let n = scores.len();

        let mle = |s: f64| -> Vec<f64> {
            let theta_epsilon = 1e-3;

            let min_theta = -1.0 / (scores[n - 1] - s);
            let max_theta = -1.0 / (scores[0] - s);
            let theta = itp(
                |x: f64| -> f64 {
                    let mut result = 0.0;
                    for i in 0..n {
                        let a_i = scores[i];
                        let phat_i = probs[i];
                        result += phat_i * (a_i - s) / (1.0 + x * (a_i - s));
                    }
                    result
                },
                min_theta,
                max_theta,
                f64::INFINITY,
                f64::NEG_INFINITY,
                0.1,
                2.0,
                0.99,
                theta_epsilon,
            );

            let mut p = vec![0.0; n];
            for i in 0..n {
                let a_i = scores[i];
                let phat_i = probs[i];
                p[i] = phat_i / (1.0 + theta * (a_i - s));
            }
            p
        };

        let p0 = mle(s0);
        let p1 = mle(s1);
        let mut lpr = vec![0.0; n];
        for i in 0..n {
            lpr[i] = p1[i].ln() - p0[i].ln();
        }
        total * mean_slice(&lpr, probs)
    }

    /// Normalized LLR computation (Section 4.1 from Van den Bergh).
    fn get_llr_normalized(
        &self,
        total: f64,
        scores: &[f64],
        probs: &[f64],
        t0: f64,
        t1: f64,
    ) -> f64 {
        let n = scores.len();

        let mle = |mu_ref: f64, t_star: f64| -> Vec<f64> {
            let theta_epsilon = 1e-7;
            let mle_epsilon = 1e-4;

            let mut p = vec![1.0 / n as f64; n];

            for _iteration in 0..10 {
                let (mu, var) = mean_and_variance_slice(scores, &p);
                let sigma = var.sqrt();
                let mut phi = vec![0.0; n];
                for i in 0..n {
                    let a_i = scores[i];
                    phi[i] = a_i
                        - mu_ref
                        - 0.5
                            * t_star
                            * sigma
                            * (1.0 + ((a_i - mu) / sigma) * ((a_i - mu) / sigma));
                }

                let u = phi.iter().cloned().fold(f64::INFINITY, f64::min);
                let v = phi.iter().cloned().fold(f64::NEG_INFINITY, f64::max);
                let min_theta = -1.0 / v;
                let max_theta = -1.0 / u;

                let theta = itp(
                    |x: f64| -> f64 {
                        let mut result = 0.0;
                        for i in 0..n {
                            let phat_i = probs[i];
                            result += phat_i * phi[i] / (1.0 + x * phi[i]);
                        }
                        result
                    },
                    min_theta,
                    max_theta,
                    f64::INFINITY,
                    f64::NEG_INFINITY,
                    0.1,
                    2.0,
                    0.99,
                    theta_epsilon,
                );

                let mut max_diff: f64 = 0.0;
                for i in 0..n {
                    let phat_i = probs[i];
                    let newp_i = phat_i / (1.0 + theta * phi[i]);
                    max_diff = max_diff.max((newp_i - p[i]).abs());
                    p[i] = newp_i;
                }

                if max_diff < mle_epsilon {
                    break;
                }
            }

            p
        };

        let p0 = mle(0.5, t0);
        let p1 = mle(0.5, t1);
        let mut lpr = vec![0.0; n];
        for i in 0..n {
            lpr[i] = p1[i].ln() - p0[i].ln();
        }
        total * mean_slice(&lpr, probs)
    }
}

/// Regularize a count value: replace 0 with a small epsilon.
fn regularize(value: i32) -> f64 {
    if value == 0 {
        1e-3
    } else {
        value as f64
    }
}

/// Weighted mean of x with weights p (slices).
fn mean_slice(x: &[f64], p: &[f64]) -> f64 {
    x.iter().zip(p.iter()).map(|(xi, pi)| xi * pi).sum()
}

/// Weighted mean and variance.
fn mean_and_variance_slice(x: &[f64], p: &[f64]) -> (f64, f64) {
    let mu = mean_slice(x, p);
    let var: f64 = x
        .iter()
        .zip(p.iter())
        .map(|(xi, pi)| pi * (xi - mu) * (xi - mu))
        .sum();
    (mu, var)
}

/// ITP root-finding method.
///
/// Oliveira & Takahashi (2020), "An Enhancement of the Bisection Method
/// Average Performance Preserving Minmax Optimality".
fn itp<F>(
    f: F,
    mut a: f64,
    mut b: f64,
    mut f_a: f64,
    mut f_b: f64,
    k_1: f64,
    k_2: f64,
    n_0: f64,
    epsilon: f64,
) -> f64
where
    F: Fn(f64) -> f64,
{
    if f_a > 0.0 {
        std::mem::swap(&mut a, &mut b);
        std::mem::swap(&mut f_a, &mut f_b);
    }
    debug_assert!(f_a < 0.0 && 0.0 < f_b);

    let n_half = ((b - a).abs() / (2.0 * epsilon)).log2().ceil();
    let n_max = n_half + n_0;
    let mut i = 0u64;
    while (b - a).abs() > 2.0 * epsilon {
        let x_half = (a + b) / 2.0;
        let r = epsilon * 2.0_f64.powf(n_max - i as f64) - (b - a) / 2.0;
        let delta = k_1 * (b - a).powf(k_2);

        let x_f = (f_b * a - f_a * b) / (f_b - f_a);

        let sigma = if (x_half - x_f).abs() > 0.0 {
            (x_half - x_f) / (x_half - x_f).abs()
        } else {
            0.0
        };
        let x_t = if delta <= (x_half - x_f).abs() {
            x_f + sigma * delta
        } else {
            x_half
        };

        let x_itp = if (x_t - x_half).abs() <= r {
            x_t
        } else {
            x_half - sigma * r
        };

        let f_itp = f(x_itp);
        if f_itp == 0.0 {
            a = x_itp;
            b = x_itp;
        } else if f_itp.is_sign_negative() {
            a = x_itp;
            f_a = f_itp;
        } else {
            b = x_itp;
            f_b = f_itp;
        }

        i += 1;
    }

    (a + b) / 2.0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sprt_disabled() {
        let sprt = Sprt::default();
        assert!(!sprt.is_enabled());
        let stats = Stats::new(100, 100, 100);
        assert_eq!(sprt.get_llr(&stats, false), 0.0);
        assert_eq!(sprt.get_result(0.0), SprtResult::Continue);
    }

    #[test]
    fn test_sprt_bounds() {
        let sprt = Sprt::new(0.05, 0.05, 0.0, 5.0, SprtModel::Normalized, true);
        assert!(sprt.is_enabled());
        // lower should be negative, upper should be positive
        assert!(sprt.lower_bound() < 0.0);
        assert!(sprt.upper_bound() > 0.0);
    }

    #[test]
    fn test_sprt_h1_with_overwhelming_wins() {
        let sprt = Sprt::new(0.05, 0.05, 0.0, 5.0, SprtModel::Logistic, true);
        let stats = Stats::new(1000, 10, 100);
        let llr = sprt.get_llr(&stats, false);
        // With overwhelming wins, LLR should be very positive → H1
        assert_eq!(sprt.get_result(llr), SprtResult::H1);
    }

    #[test]
    fn test_sprt_h0_with_balanced() {
        // Use a wider elo range and more games for a clearer H0 signal.
        // With perfectly balanced results, the LLR should be strongly negative.
        let sprt = Sprt::new(0.05, 0.05, 0.0, 10.0, SprtModel::Logistic, true);
        let stats = Stats::new(10000, 10000, 10000);
        let llr = sprt.get_llr(&stats, false);
        assert_eq!(sprt.get_result(llr), SprtResult::H0);
    }

    #[test]
    fn test_sprt_continue_with_few_games() {
        let sprt = Sprt::new(0.05, 0.05, 0.0, 5.0, SprtModel::Normalized, true);
        let stats = Stats::new(5, 4, 3);
        let llr = sprt.get_llr(&stats, false);
        // With very few games, we should get Continue
        assert_eq!(sprt.get_result(llr), SprtResult::Continue);
    }

    #[test]
    fn test_sprt_pentanomial() {
        let sprt = Sprt::new(0.05, 0.05, 0.0, 5.0, SprtModel::Normalized, true);
        let stats = Stats::from_pentanomial(10, 50, 100, 200, 50, 10);
        let llr = sprt.get_llr(&stats, true);
        // Should produce a finite LLR value
        assert!(llr.is_finite());
    }

    #[test]
    fn test_validate() {
        let mut penta = true;
        assert!(Sprt::validate(0.05, 0.05, 0.0, 5.0, "normalized", &mut penta).is_ok());
        assert!(penta);

        assert!(Sprt::validate(0.05, 0.05, 5.0, 0.0, "normalized", &mut penta).is_err());
        assert!(Sprt::validate(0.0, 0.05, 0.0, 5.0, "normalized", &mut penta).is_err());
        assert!(Sprt::validate(0.05, 0.05, 0.0, 5.0, "invalid", &mut penta).is_err());

        let mut penta2 = true;
        assert!(Sprt::validate(0.05, 0.05, 0.0, 5.0, "bayesian", &mut penta2).is_ok());
        assert!(!penta2); // bayesian disables pentanomial
    }

    #[test]
    fn test_score_conversions() {
        // lelo_to_score(0) should be 0.5 (equal opponents)
        assert!((Sprt::lelo_to_score(0.0) - 0.5).abs() < 1e-10);
        // Positive elo → score > 0.5
        assert!(Sprt::lelo_to_score(100.0) > 0.5);
        // Negative elo → score < 0.5
        assert!(Sprt::lelo_to_score(-100.0) < 0.5);

        // bayeselo_to_score with drawelo=0 should match lelo
        let score_b = Sprt::bayeselo_to_score(100.0, 0.0);
        let score_l = Sprt::lelo_to_score(100.0);
        assert!((score_b - score_l).abs() < 0.01);
    }

    #[test]
    fn test_format_strings() {
        let sprt = Sprt::new(0.05, 0.05, 0.0, 5.0, SprtModel::Normalized, true);
        let bounds = sprt.get_bounds();
        assert!(bounds.starts_with('('));
        assert!(bounds.ends_with(')'));

        let elo = sprt.get_elo();
        assert!(elo.starts_with('['));
        assert!(elo.ends_with(']'));
        assert!(elo.contains("0.00"));
        assert!(elo.contains("5.00"));
    }
}
