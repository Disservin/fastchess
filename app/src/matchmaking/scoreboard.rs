//! Scoreboard for tracking per-pair engine match statistics.
//!
//! Ports `matchmaking/scoreboard.hpp` from C++.

use std::collections::HashMap;
use std::sync::Mutex;

use serde::{Deserialize, Serialize};

use super::stats::Stats;
use crate::matchmaking::tournament::runner::GameAssignment;

/// A key identifying a pair of players (white, black).
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct PlayerPairKey {
    pub first: String,
    pub second: String,
}

impl PlayerPairKey {
    pub fn new(first: impl Into<String>, second: impl Into<String>) -> Self {
        Self {
            first: first.into(),
            second: second.into(),
        }
    }
}

/// Map from player pairs to their stats.
pub type StatsMap = HashMap<PlayerPairKey, Stats>;

/// Serialize a StatsMap as "player1 vs player2" → Stats.
pub fn stats_map_to_json(map: &StatsMap) -> serde_json::Value {
    let mut obj = serde_json::Map::new();
    for (key, value) in map {
        // Use tuple key directly instead of string concatenation
        obj.insert(
            format!("{} vs {}", key.first, key.second),
            serde_json::to_value(value).unwrap_or_default(),
        );
    }
    serde_json::Value::Object(obj)
}

/// Deserialize a StatsMap from JSON.
pub fn stats_map_from_json(j: &serde_json::Value) -> StatsMap {
    let mut map = StatsMap::new();
    if let Some(obj) = j.as_object() {
        for (key, value) in obj {
            if let Some((first, second)) = key.split_once(" vs ") {
                if let Ok(stats) = serde_json::from_value::<Stats>(value.clone()) {
                    map.insert(
                        PlayerPairKey::new(first.to_string(), second.to_string()),
                        stats,
                    );
                }
            }
        }
    }
    map
}

/// Accessor wrapper around a StatsMap with operator-like indexing.
#[derive(Debug, Default)]
pub struct StatsMapWrapper {
    results: StatsMap,
}

impl StatsMapWrapper {
    pub fn get_or_insert(&mut self, configs: &GameAssignment) -> &mut Stats {
        let key = PlayerPairKey::new(
            configs.first_name().to_owned(),
            configs.second_name().to_owned(),
        );
        self.results.entry(key).or_default()
    }

    pub fn get(&self, configs: &GameAssignment) -> Option<&Stats> {
        let key = PlayerPairKey::new(
            configs.first_name().to_owned(),
            configs.second_name().to_owned(),
        );
        self.results.get(&key)
    }

    pub fn get_by_names(&mut self, first: &str, second: &str) -> &mut Stats {
        let key = PlayerPairKey::new(first, second);
        self.results.entry(key).or_default()
    }

    pub fn get_by_names_ref(&self, first: &str, second: &str) -> Option<&Stats> {
        let key = PlayerPairKey::new(first, second);
        self.results.get(&key)
    }

    pub fn iter(&self) -> impl Iterator<Item = (&PlayerPairKey, &Stats)> {
        self.results.iter()
    }

    pub fn inner(&self) -> &StatsMap {
        &self.results
    }

    pub fn set_inner(&mut self, results: StatsMap) {
        self.results = results;
    }
}

/// Thread-safe scoreboard tracking engine match results with pentanomial pair support.
pub struct ScoreBoard {
    results: Mutex<StatsMapWrapper>,
    /// Cache for in-progress game pairs (keyed by round_id).
    game_pair_cache: Mutex<HashMap<u64, Stats>>,
}

impl ScoreBoard {
    pub fn new() -> Self {
        Self {
            results: Mutex::new(StatsMapWrapper::default()),
            game_pair_cache: Mutex::new(HashMap::new()),
        }
    }

    /// Returns true if the pair for this round has been completed (both games done).
    pub fn is_pair_completed(&self, round_id: u64) -> bool {
        let cache = self.game_pair_cache.lock().unwrap();
        !cache.contains_key(&round_id)
    }

    /// Update stats directly (no pair tracking). Always returns true.
    pub fn update_non_pair(&self, configs: &GameAssignment, stats: &Stats) -> bool {
        let mut results = self.results.lock().unwrap();
        let entry = results.get_or_insert(configs);
        *entry += stats;
        true
    }

    /// Update stats in pair batches to track pentanomial statistics.
    ///
    /// Returns true if the pair was completed (second game of the pair),
    /// false if this was the first game.
    pub fn update_pair(&self, configs: &GameAssignment, stats: &Stats, round_id: u64) -> bool {
        let mut cache = self.game_pair_cache.lock().unwrap();

        let is_first_game = !cache.contains_key(&round_id);

        if is_first_game {
            // Invert because otherwise the stats would be from the same perspective
            // for different players
            cache.insert(round_id, stats.inverted());
            return false;
        }

        let lookup = cache.get_mut(&round_id).unwrap();
        *lookup += stats;

        // Compute pentanomial category
        let wins = lookup.wins;
        let losses = lookup.losses;
        let draws = lookup.draws;

        lookup.penta_ww += (wins == 2) as i32;
        lookup.penta_wd += (wins == 1 && draws == 1) as i32;
        lookup.penta_wl += (wins == 1 && losses == 1) as i32;
        lookup.penta_dd += (draws == 2) as i32;
        lookup.penta_ld += (losses == 1 && draws == 1) as i32;
        lookup.penta_ll += (losses == 2) as i32;

        let pair_stats = lookup.clone();
        cache.remove(&round_id);

        // Drop cache lock before acquiring results lock
        drop(cache);

        // Update the main results
        let mut results = self.results.lock().unwrap();
        let entry = results.get_or_insert(configs);
        *entry += pair_stats;

        true
    }

    /// Get combined stats for engine1 vs engine2 (from both color perspectives).
    pub fn get_stats(&self, engine1: &str, engine2: &str) -> Stats {
        let results = self.results.lock().unwrap();

        let stats1 = results
            .get_by_names_ref(engine1, engine2)
            .cloned()
            .unwrap_or_default();
        let stats2 = results
            .get_by_names_ref(engine2, engine1)
            .cloned()
            .unwrap_or_default();

        stats1 + stats2.inverted()
    }

    /// Get all stats for an engine across all opponents.
    pub fn get_all_stats(&self, engine: &str) -> Stats {
        let results = self.results.lock().unwrap();
        let mut stats = Stats::default();

        for (key, value) in results.iter() {
            if key.first == engine {
                stats += value;
            } else if key.second == engine {
                stats += value.inverted();
            }
        }

        stats
    }

    /// Get a copy of the underlying results map.
    pub fn get_results(&self) -> StatsMap {
        let results = self.results.lock().unwrap();
        results.inner().clone()
    }

    /// Replace the underlying results map.
    pub fn set_results(&self, new_results: StatsMap) {
        let mut results = self.results.lock().unwrap();
        results.set_inner(new_results);
    }
}

impl Default for ScoreBoard {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::{
        matchmaking::tournament::runner::GameAssignment, types::engine_config::EngineConfiguration,
    };

    static W_CFG: std::sync::LazyLock<EngineConfiguration> =
        std::sync::LazyLock::new(|| create_config("engine1"));
    static B_CFG: std::sync::LazyLock<EngineConfiguration> =
        std::sync::LazyLock::new(|| create_config("engine2"));
    static C_CFG: std::sync::LazyLock<EngineConfiguration> =
        std::sync::LazyLock::new(|| create_config("engine3"));

    fn create_config(name: &str) -> EngineConfiguration {
        let mut cfg = EngineConfiguration::default();
        cfg.name = name.to_string();
        cfg
    }

    #[test]
    fn test_update_non_pair() {
        let board = ScoreBoard::new();

        let configs = GameAssignment::new(&*W_CFG, &*B_CFG);

        let stats = Stats::new(1, 0, 0); // engine1 wins

        board.update_non_pair(&configs, &stats);

        let result = board.get_stats("engine1", "engine2");
        assert_eq!(result.wins, 1);
        assert_eq!(result.losses, 0);
    }

    #[test]
    fn test_update_pair() {
        let board = ScoreBoard::new();
        let configs = GameAssignment::new(&*W_CFG, &*B_CFG);

        // First game: engine1 wins as white
        let stats1 = Stats::new(1, 0, 0);
        let completed = board.update_pair(&configs, &stats1, 1);
        assert!(!completed, "First game of pair should not complete it");

        // Second game: engine1 draws as white
        let stats2 = Stats::new(0, 0, 1);
        let completed = board.update_pair(&configs, &stats2, 1);
        assert!(completed, "Second game of pair should complete it");

        // Pair should now be completed
        assert!(board.is_pair_completed(1));
    }

    #[test]
    fn test_get_all_stats() {
        let board = ScoreBoard::new();

        let configs1 = GameAssignment::new(&*W_CFG, &*B_CFG);
        let configs2 = GameAssignment::new(&*W_CFG, &*C_CFG);

        board.update_non_pair(&configs1, &Stats::new(3, 1, 2));
        board.update_non_pair(&configs2, &Stats::new(2, 2, 1));

        let all = board.get_all_stats("engine1");
        assert_eq!(all.wins, 5);
        assert_eq!(all.losses, 3);
        assert_eq!(all.draws, 3);
    }

    #[test]
    fn test_stats_map_serialization() {
        let mut map = StatsMap::new();
        map.insert(
            PlayerPairKey::new("engine1", "engine2"),
            Stats::new(10, 5, 8),
        );

        let json = stats_map_to_json(&map);
        let restored = stats_map_from_json(&json);

        let key = PlayerPairKey::new("engine1", "engine2");
        assert_eq!(restored.get(&key).unwrap().wins, 10);
        assert_eq!(restored.get(&key).unwrap().losses, 5);
        assert_eq!(restored.get(&key).unwrap().draws, 8);
    }

    #[test]
    fn test_pentanomial_pair_tracking() {
        let board = ScoreBoard::new();
        let configs = GameAssignment::new(&*W_CFG, &*B_CFG);

        // Game pair where engine1 wins both games
        let win_stats = Stats::new(1, 0, 0);
        board.update_pair(&configs, &win_stats, 1); // first game
        board.update_pair(&configs, &win_stats, 1); // second game

        let result = board.get_results();
        let key = PlayerPairKey::new("engine1", "engine2");
        let stats = result.get(&key).unwrap();
        // After inversion of first + second win, we get WW
        // First: inverted(1,0,0) = (0,1,0), then += (1,0,0) = (1,1,0)
        // That's wins=1, losses=1 → penta_WL
        assert_eq!(stats.penta_wl, 1);
    }
}
