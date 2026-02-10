//! Win/Loss/Draw statistics with pentanomial pair tracking.
//!
//! Ports `matchmaking/stats.hpp` from C++.

use serde::{Deserialize, Serialize};
use std::ops;

use crate::types::match_data::{GameResult, MatchData};

/// Win/Loss/Draw statistics with pentanomial pair tracking.
///
/// Pentanomial stats track game-pair outcomes:
/// - `penta_WW`: both games won
/// - `penta_WD`: one win, one draw
/// - `penta_WL`: one win, one loss (split)
/// - `penta_DD`: both games drawn
/// - `penta_LD`: one loss, one draw
/// - `penta_LL`: both games lost
#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct Stats {
    pub wins: i32,
    pub losses: i32,
    pub draws: i32,

    pub penta_ww: i32,
    pub penta_wd: i32,
    pub penta_wl: i32,
    pub penta_dd: i32,
    pub penta_ld: i32,
    pub penta_ll: i32,
}

impl Stats {
    pub fn new(wins: i32, losses: i32, draws: i32) -> Self {
        Self {
            wins,
            losses,
            draws,
            ..Default::default()
        }
    }

    pub fn from_pentanomial(ll: i32, ld: i32, wl: i32, dd: i32, wd: i32, ww: i32) -> Self {
        Self {
            penta_ll: ll,
            penta_ld: ld,
            penta_wl: wl,
            penta_dd: dd,
            penta_wd: wd,
            penta_ww: ww,
            ..Default::default()
        }
    }

    /// Create stats from a single match result (white player's perspective).
    pub fn from_match(match_data: &MatchData) -> Self {
        let mut stats = Self::default();
        match match_data.players.white.result {
            GameResult::Win => stats.wins += 1,
            GameResult::Lose => stats.losses += 1,
            GameResult::Draw => stats.draws += 1,
            GameResult::None => {
                // Also check black side for draw
                if match_data.players.black.result == GameResult::Draw {
                    stats.draws += 1;
                }
            }
        }
        stats
    }

    /// Invert stats (swap wins/losses, swap WW/LL, swap WD/LD).
    pub fn inverted(&self) -> Self {
        Self {
            wins: self.losses,
            losses: self.wins,
            draws: self.draws,
            penta_ww: self.penta_ll,
            penta_wd: self.penta_ld,
            penta_wl: self.penta_wl,
            penta_dd: self.penta_dd,
            penta_ld: self.penta_wd,
            penta_ll: self.penta_ww,
        }
    }

    /// Total number of games.
    pub fn sum(&self) -> i32 {
        self.wins + self.losses + self.draws
    }

    /// Total score in points (win=1, draw=0.5).
    pub fn points(&self) -> f64 {
        self.wins as f64 + 0.5 * self.draws as f64
    }

    /// Ratio of WL pairs to DD pairs.
    pub fn wl_dd_ratio(&self) -> f64 {
        self.penta_wl as f64 / self.penta_dd as f64
    }

    /// Draw ratio as a percentage.
    pub fn draw_ratio(&self) -> f64 {
        100.0 * self.draws as f64 / self.sum() as f64
    }

    /// Draw ratio computed from pentanomial pairs.
    pub fn draw_ratio_penta(&self) -> f64 {
        ((self.penta_wl + self.penta_dd) as f64 / self.total_pairs() as f64) * 100.0
    }

    /// Ratio of winning pairs to losing pairs.
    pub fn pairs_ratio(&self) -> f64 {
        (self.penta_ww + self.penta_wd) as f64 / (self.penta_ld + self.penta_ll) as f64
    }

    /// Points as a percentage of total games.
    pub fn points_ratio(&self) -> f64 {
        self.points() / self.sum() as f64 * 100.0
    }

    /// Total number of game pairs.
    pub fn total_pairs(&self) -> i32 {
        self.penta_ww
            + self.penta_wd
            + self.penta_wl
            + self.penta_dd
            + self.penta_ld
            + self.penta_ll
    }
}

impl ops::Add for Stats {
    type Output = Self;

    fn add(self, rhs: Self) -> Self {
        Self {
            wins: self.wins + rhs.wins,
            losses: self.losses + rhs.losses,
            draws: self.draws + rhs.draws,
            penta_ww: self.penta_ww + rhs.penta_ww,
            penta_wd: self.penta_wd + rhs.penta_wd,
            penta_wl: self.penta_wl + rhs.penta_wl,
            penta_dd: self.penta_dd + rhs.penta_dd,
            penta_ld: self.penta_ld + rhs.penta_ld,
            penta_ll: self.penta_ll + rhs.penta_ll,
        }
    }
}

impl ops::AddAssign for Stats {
    fn add_assign(&mut self, rhs: Self) {
        self.wins += rhs.wins;
        self.losses += rhs.losses;
        self.draws += rhs.draws;
        self.penta_ww += rhs.penta_ww;
        self.penta_wd += rhs.penta_wd;
        self.penta_wl += rhs.penta_wl;
        self.penta_dd += rhs.penta_dd;
        self.penta_ld += rhs.penta_ld;
        self.penta_ll += rhs.penta_ll;
    }
}

impl ops::AddAssign<&Stats> for Stats {
    fn add_assign(&mut self, rhs: &Stats) {
        self.wins += rhs.wins;
        self.losses += rhs.losses;
        self.draws += rhs.draws;
        self.penta_ww += rhs.penta_ww;
        self.penta_wd += rhs.penta_wd;
        self.penta_wl += rhs.penta_wl;
        self.penta_dd += rhs.penta_dd;
        self.penta_ld += rhs.penta_ld;
        self.penta_ll += rhs.penta_ll;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_stats_basic() {
        let s = Stats::new(10, 5, 3);
        assert_eq!(s.sum(), 18);
        assert!((s.points() - 11.5).abs() < f64::EPSILON);
    }

    #[test]
    fn test_stats_add() {
        let a = Stats::new(5, 3, 2);
        let b = Stats::new(3, 1, 4);
        let c = a + b;
        assert_eq!(c.wins, 8);
        assert_eq!(c.losses, 4);
        assert_eq!(c.draws, 6);
    }

    #[test]
    fn test_stats_invert() {
        let s = Stats {
            wins: 10,
            losses: 5,
            draws: 3,
            penta_ww: 2,
            penta_wd: 1,
            penta_wl: 3,
            penta_dd: 4,
            penta_ld: 0,
            penta_ll: 1,
        };
        let inv = s.inverted();
        assert_eq!(inv.wins, 5);
        assert_eq!(inv.losses, 10);
        assert_eq!(inv.draws, 3);
        assert_eq!(inv.penta_ww, 1); // was LL
        assert_eq!(inv.penta_ll, 2); // was WW
        assert_eq!(inv.penta_wd, 0); // was LD
        assert_eq!(inv.penta_ld, 1); // was WD
        assert_eq!(inv.penta_wl, 3); // WL stays
        assert_eq!(inv.penta_dd, 4); // DD stays
    }

    #[test]
    fn test_stats_serialization() {
        let s = Stats::new(10, 5, 3);
        let json = serde_json::to_string(&s).unwrap();
        let deserialized: Stats = serde_json::from_str(&json).unwrap();
        assert_eq!(s, deserialized);
    }

    #[test]
    fn test_pentanomial() {
        let s = Stats::from_pentanomial(1, 2, 3, 4, 5, 6);
        assert_eq!(s.total_pairs(), 21);
    }
}
