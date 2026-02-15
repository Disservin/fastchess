//! Engine cache — reuses engine processes across games.
//!
//! Ports `core/memory/cache.hpp`, `core/memory/scope_guard.hpp`, and the
//! cache integration from `matchmaking/tournament/base/tournament.hpp/cpp`.
//!
//! ## Design
//!
//! * **`CachePool`** — a thread-safe pool of `UciEngine` instances, keyed by
//!   engine name.  Each entry is individually `Mutex`-protected so that
//!   checking out one engine doesn't block access to another.
//!
//! * **`EngineGuard`** — RAII guard returned by [`EngineCache::get_engine`].
//!   Holds a lock on the individual entry and provides `&mut UciEngine`
//!   access.  On drop it either marks the slot available (normal) or removes
//!   it from the pool (`restart=on`).
//!
//! * **`EngineCache`** — top-level facade that dispatches to a single shared
//!   `CachePool` (no affinity) or per-thread `CachePool`s (with affinity).

use std::collections::HashMap;
use std::sync::{Arc, Mutex, MutexGuard};
use std::thread;

use crate::engine::process::ProcessError;
use crate::engine::uci_engine::UciEngine;
use crate::types::engine_config::EngineConfiguration;

// ── CachedEntry ──────────────────────────────────────────────────────────────

/// A single engine slot in the pool.
///
/// Each entry has its own `Mutex` so that two engines from the same pool can
/// be used simultaneously (e.g., white and black in a game).
struct EntryInner {
    engine: UciEngine,
    name: String,
    available: bool,
}

type Entry = Arc<Mutex<EntryInner>>;

// ── CachePool ────────────────────────────────────────────────────────────────

/// Thread-safe pool of `UciEngine` instances identified by name.
///
/// Mirrors C++ `CachePool<UciEngine, std::string>`.
///
/// The pool uses a two-level locking scheme:
/// - A top-level `Mutex<Vec<Entry>>` protects the list of entries (for
///   add/remove/scan operations).
/// - Each `Entry` has its own `Mutex<EntryInner>` for accessing the engine.
///
/// The top-level lock is held only briefly (to scan for an available entry
/// or append a new one).  The per-entry lock is held for the duration of the
/// game (via `EngineGuard`).
pub struct CachePool {
    entries: Mutex<Vec<Entry>>,
}

impl Default for CachePool {
    fn default() -> Self {
        Self::new()
    }
}

impl CachePool {
    pub fn new() -> Self {
        Self {
            entries: Mutex::new(Vec::new()),
        }
    }

    /// Check out an engine with the given `name`.
    ///
    /// If an available entry with a matching name exists, it is reused.
    /// Otherwise a new `UciEngine` is constructed and appended to the pool.
    ///
    /// Returns the `Arc<Mutex<EntryInner>>` for the checked-out entry.
    fn get_entry(
        &self,
        name: &str,
        config: &EngineConfiguration,
        realtime_logging: bool,
    ) -> Result<Entry, ProcessError> {
        let mut entries = self.entries.lock().unwrap();

        // Look for an available entry with matching name.
        // We lock each entry briefly to check availability.
        for entry in entries.iter() {
            let mut inner = entry.lock().unwrap();
            if inner.available && inner.name == name {
                inner.available = false;
                return Ok(Arc::clone(entry));
            }
        }

        // Not found — create a new engine
        let engine = UciEngine::new(config, realtime_logging)?;
        let entry = Arc::new(Mutex::new(EntryInner {
            engine,
            name: name.to_string(),
            available: false,
        }));
        entries.push(Arc::clone(&entry));
        Ok(entry)
    }

    /// Mark an entry as available (returns the engine to the pool).
    fn release(entry: &Entry) {
        let mut inner = entry.lock().unwrap();
        inner.available = true;
    }

    /// Remove an entry from the pool (for `restart=on` mode).
    ///
    /// The engine's `Drop` impl sends `stop` + `quit` to the process.
    fn delete_entry(&self, entry: &Entry) {
        let mut entries = self.entries.lock().unwrap();
        entries.retain(|e| !Arc::ptr_eq(e, entry));
        // The entry's engine will be dropped when the last Arc ref goes away
        // (i.e., when the EngineGuard that holds it is also dropped).
    }
}

// ── EngineGuard ──────────────────────────────────────────────────────────────

/// RAII guard for a checked-out engine.
///
/// Provides direct `&UciEngine` and `&mut UciEngine` access via `Deref` /
/// `DerefMut`.  On drop, either releases the entry back to the pool or
/// deletes it (if `restart=on`).
pub struct EngineGuard {
    pool: Arc<CachePool>,
    entry: Entry,
    restart: bool,
}

impl EngineGuard {
    /// Get a locked reference to the inner entry.
    ///
    /// Callers use this to access the engine.  The `MutexGuard` is held for
    /// the duration of the returned reference — but since only one thread
    /// owns the `EngineGuard` at a time, there's no contention.
    pub fn lock(&self) -> EngineRef<'_> {
        EngineRef {
            guard: self.entry.lock().unwrap(),
        }
    }

    /// Restart the engine process in-place (for crash recovery).
    ///
    /// The old engine is dropped (sends quit) and replaced with a fresh one.
    pub fn restart_engine(&self) -> Result<(), ProcessError> {
        let mut inner = self.entry.lock().unwrap();
        let config = inner.engine.config().clone();
        let rl = inner.engine.is_realtime_logging();
        inner.engine = UciEngine::new(&config, rl)?;
        Ok(())
    }
}

impl Drop for EngineGuard {
    fn drop(&mut self) {
        if self.restart {
            self.pool.delete_entry(&self.entry);
        } else {
            CachePool::release(&self.entry);
        }
    }
}

/// Locked reference to a `UciEngine` inside a cache entry.
///
/// Implements `Deref<Target = UciEngine>` and `DerefMut` for ergonomic access.
pub struct EngineRef<'a> {
    guard: MutexGuard<'a, EntryInner>,
}

impl<'a> std::ops::Deref for EngineRef<'a> {
    type Target = UciEngine;
    fn deref(&self) -> &UciEngine {
        &self.guard.engine
    }
}

impl<'a> std::ops::DerefMut for EngineRef<'a> {
    fn deref_mut(&mut self) -> &mut UciEngine {
        &mut self.guard.engine
    }
}

// ── EngineCache ──────────────────────────────────────────────────────────────

/// Top-level engine cache.
///
/// - **Without affinity** (`affinity = false`): single shared `CachePool`.
///   Any worker thread may reuse any engine.
/// - **With affinity** (`affinity = true`): per-`ThreadId` pools.  Engines
///   stay pinned to the thread (and thus CPU cores) they were created on.
pub struct EngineCache {
    affinity: bool,
    global_cache: Arc<CachePool>,
    thread_caches: Mutex<HashMap<thread::ThreadId, Arc<CachePool>>>,
}

impl EngineCache {
    pub fn new(affinity: bool) -> Self {
        Self {
            affinity,
            global_cache: Arc::new(CachePool::new()),
            thread_caches: Mutex::new(HashMap::new()),
        }
    }

    /// Check out an engine, returning an RAII guard.
    ///
    /// The guard provides `&mut UciEngine` access via [`EngineGuard::lock`]
    /// and automatically returns (or deletes) the engine when dropped.
    pub fn get_engine(
        &self,
        name: &str,
        config: &EngineConfiguration,
        realtime_logging: bool,
    ) -> Result<EngineGuard, ProcessError> {
        let pool = self.get_pool();
        let entry = pool.get_entry(name, config, realtime_logging)?;
        Ok(EngineGuard {
            pool,
            entry,
            restart: config.restart,
        })
    }

    /// Get the appropriate pool for the calling thread.
    fn get_pool(&self) -> Arc<CachePool> {
        if !self.affinity {
            Arc::clone(&self.global_cache)
        } else {
            let tid = thread::current().id();
            let mut map = self.thread_caches.lock().unwrap();
            let pool = map.entry(tid).or_insert_with(|| Arc::new(CachePool::new()));
            Arc::clone(pool)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::engine_config::EngineConfiguration;

    fn dummy_config(name: &str) -> EngineConfiguration {
        EngineConfiguration {
            name: name.to_string(),
            ..Default::default()
        }
    }

    fn restart_config(name: &str) -> EngineConfiguration {
        EngineConfiguration {
            name: name.to_string(),
            restart: true,
            ..Default::default()
        }
    }

    #[test]
    fn test_cache_pool_reuse() {
        let pool = Arc::new(CachePool::new());
        let config = dummy_config("engine1");

        // First checkout creates a new entry
        let entry1 = pool
            .get_entry("engine1", &config, false)
            .expect("Failed to get entry");
        {
            let inner = entry1.lock().unwrap();
            assert!(!inner.available);
            assert_eq!(inner.name, "engine1");
        }

        // Release it
        CachePool::release(&entry1);

        // Second checkout should reuse the same entry (same Arc)
        let entry2 = pool
            .get_entry("engine1", &config, false)
            .expect("Failed to get entry");
        assert!(Arc::ptr_eq(&entry1, &entry2));

        CachePool::release(&entry2);
    }

    #[test]
    fn test_cache_pool_different_names() {
        let pool = Arc::new(CachePool::new());
        let config1 = dummy_config("engine1");
        let config2 = dummy_config("engine2");

        let entry1 = pool
            .get_entry("engine1", &config1, false)
            .expect("Failed to get entry");
        let entry2 = pool
            .get_entry("engine2", &config2, false)
            .expect("Failed to get entry");

        // Different entries
        assert!(!Arc::ptr_eq(&entry1, &entry2));

        CachePool::release(&entry1);
        CachePool::release(&entry2);
    }

    #[test]
    fn test_cache_pool_no_reuse_when_unavailable() {
        let pool = Arc::new(CachePool::new());
        let config = dummy_config("engine1");

        // Checkout without releasing — should create a second entry
        let entry1 = pool
            .get_entry("engine1", &config, false)
            .expect("Failed to get entry");
        let entry2 = pool
            .get_entry("engine1", &config, false)
            .expect("Failed to get entry");

        assert!(!Arc::ptr_eq(&entry1, &entry2));

        CachePool::release(&entry1);
        CachePool::release(&entry2);
    }

    #[test]
    fn test_cache_pool_delete_entry() {
        let pool = Arc::new(CachePool::new());
        let config = restart_config("engine1");

        let entry = pool
            .get_entry("engine1", &config, false)
            .expect("Failed to get entry");

        // Delete instead of release
        pool.delete_entry(&entry);

        // Pool should be empty — next checkout creates a new entry
        let entry2 = pool
            .get_entry("engine1", &config, false)
            .expect("Failed to get entry");
        assert!(!Arc::ptr_eq(&entry, &entry2));

        pool.delete_entry(&entry2);
    }

    #[test]
    fn test_engine_guard_releases_on_drop() {
        let pool = Arc::new(CachePool::new());
        let config = dummy_config("engine1");

        let entry_clone;
        {
            let entry = pool
                .get_entry("engine1", &config, false)
                .expect("Failed to get entry");
            entry_clone = Arc::clone(&entry);
            let _guard = EngineGuard {
                pool: Arc::clone(&pool),
                entry,
                restart: false,
            };
            // Guard drops here, releasing the entry
        }

        // Entry should be available now
        let inner = entry_clone.lock().unwrap();
        assert!(inner.available);
    }

    #[test]
    fn test_engine_guard_deletes_on_drop_restart() {
        let pool = Arc::new(CachePool::new());
        let config = restart_config("engine1");

        {
            let entry = pool
                .get_entry("engine1", &config, false)
                .expect("Failed to get entry");
            let _guard = EngineGuard {
                pool: Arc::clone(&pool),
                entry,
                restart: true,
            };
            // Guard drops here, deleting the entry from pool
        }

        // Pool should be empty
        let entries = pool.entries.lock().unwrap();
        assert!(entries.is_empty());
    }

    #[test]
    fn test_engine_cache_global_mode() {
        let cache = EngineCache::new(false);
        let config = dummy_config("engine1");

        {
            let _guard = cache
                .get_engine("engine1", &config, false)
                .expect("Failed to get engine");
        }

        // Should reuse — just verify no panic
        {
            let _guard = cache
                .get_engine("engine1", &config, false)
                .expect("Failed to get engine");
        }
    }

    #[test]
    fn test_engine_cache_affinity_mode() {
        let cache = EngineCache::new(true);
        let config = dummy_config("engine1");

        {
            let _guard = cache
                .get_engine("engine1", &config, false)
                .expect("Failed to get engine");
        }

        // Same thread — should reuse
        {
            let _guard = cache
                .get_engine("engine1", &config, false)
                .expect("Failed to get engine");
        }
    }

    #[test]
    fn test_engine_guard_lock_access() {
        let pool = Arc::new(CachePool::new());
        let config = dummy_config("test_engine");

        let entry = pool
            .get_entry("test_engine", &config, false)
            .expect("Failed to get entry");
        let guard = EngineGuard {
            pool: Arc::clone(&pool),
            entry,
            restart: false,
        };

        // Should be able to access the engine through lock()
        {
            let engine_ref = guard.lock();
            assert_eq!(engine_ref.config().name, "test_engine");
        }
    }

    #[test]
    fn test_two_engines_simultaneously() {
        let pool = Arc::new(CachePool::new());
        let config1 = dummy_config("white");
        let config2 = dummy_config("black");

        // Check out two engines at the same time (like a game)
        let entry1 = pool
            .get_entry("white", &config1, false)
            .expect("Failed to get entry");
        let entry2 = pool
            .get_entry("black", &config2, false)
            .expect("Failed to get entry");

        let guard1 = EngineGuard {
            pool: Arc::clone(&pool),
            entry: entry1,
            restart: false,
        };
        let guard2 = EngineGuard {
            pool: Arc::clone(&pool),
            entry: entry2,
            restart: false,
        };

        // Lock both simultaneously — no deadlock because each entry has
        // its own mutex
        {
            let ref1 = guard1.lock();
            let ref2 = guard2.lock();
            assert_eq!(ref1.config().name, "white");
            assert_eq!(ref2.config().name, "black");
        }
    }
}
