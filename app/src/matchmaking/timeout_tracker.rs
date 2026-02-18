//! Per-player timeout and disconnect tracking.
//!
//! Ports `matchmaking/timeout_tracker.hpp` from C++.

use std::collections::HashMap;
use std::sync::Mutex;

/// Tracks timeout and disconnect counts for a single player.
#[derive(Debug, Default)]
pub struct Tracker {
    pub timeouts: usize,
    pub disconnects: usize,
}

/// Thread-safe tracker for per-player timeout and disconnect events.
///
/// Used by the tournament to decide whether an engine should be retired.
pub struct PlayerTracker {
    counts: Mutex<HashMap<String, Tracker>>,
}

impl PlayerTracker {
    pub fn new() -> Self {
        Self {
            counts: Mutex::new(HashMap::new()),
        }
    }

    /// Record a timeout event for a player.
    pub fn report_timeout(&self, player: &str) {
        let mut counts = self.counts.lock().unwrap();
        counts.entry(player.to_string()).or_default().timeouts += 1;
    }

    /// Record a disconnect event for a player.
    pub fn report_disconnect(&self, player: &str) {
        let mut counts = self.counts.lock().unwrap();
        counts.entry(player.to_string()).or_default().disconnects += 1;
    }

    /// Iterate over all tracked players and their counts.
    pub fn iter(&self) -> Vec<(String, usize, usize)> {
        let counts = self.counts.lock().unwrap();
        counts
            .iter()
            .map(|(name, t)| (name.clone(), t.timeouts, t.disconnects))
            .collect()
    }

    /// Reset all counters.
    pub fn reset_all(&self) {
        let mut counts = self.counts.lock().unwrap();
        counts.clear();
    }
}

impl Default for PlayerTracker {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tracker() {
        let tracker = PlayerTracker::new();
        tracker.report_timeout("engine1");
        tracker.report_timeout("engine1");
        tracker.report_disconnect("engine2");

        let data = tracker.iter();
        let e1 = data.iter().find(|(n, _, _)| n == "engine1").unwrap();
        assert_eq!(e1.1, 2); // timeouts
        assert_eq!(e1.2, 0); // disconnects

        let e2 = data.iter().find(|(n, _, _)| n == "engine2").unwrap();
        assert_eq!(e2.1, 0);
        assert_eq!(e2.2, 1);

        tracker.reset_all();
        assert!(tracker.iter().is_empty());
    }
}
