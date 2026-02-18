//! Engine cache — reuses engine processes across games.
//!
//! Uses `dashmap` for global pools and `thread_local!` for affinity pools.

use dashmap::DashMap;
use std::cell::RefCell;
use std::collections::HashMap;
use std::ops::{Deref, DerefMut};
use std::sync::{Arc, Mutex, MutexGuard};

use crate::engine::process::ProcessError;
use crate::engine::uci_engine::UciEngine;
use crate::types::engine_config::EngineConfiguration;

thread_local! {
    static THREAD_POOLS: RefCell<HashMap<String, Arc<NamePool>>> =
        RefCell::new(HashMap::new());
}

/// A pooled engine entry with its own lock (allows concurrent use of different engines).
struct PooledEngine {
    engine: Mutex<UciEngine>,
    available: Mutex<bool>,
}

/// Pool for one engine name.
struct NamePool {
    entries: Mutex<Vec<Arc<PooledEngine>>>,
}

impl NamePool {
    fn new() -> Self {
        Self {
            entries: Mutex::new(Vec::new()),
        }
    }

    fn acquire(
        &self,
        _name: &str,
        config: &EngineConfiguration,
        realtime_logging: bool,
    ) -> Result<Arc<PooledEngine>, ProcessError> {
        let mut entries = self.entries.lock().unwrap();

        // Find available entry
        for entry in entries.iter() {
            if *entry.available.lock().unwrap() {
                *entry.available.lock().unwrap() = false;
                return Ok(Arc::clone(entry));
            }
        }

        // Create new
        let engine = UciEngine::new(config, realtime_logging)?;
        let entry = Arc::new(PooledEngine {
            engine: Mutex::new(engine),
            available: Mutex::new(false),
        });
        entries.push(Arc::clone(&entry));
        Ok(entry)
    }

    fn release(&self, entry: &PooledEngine) {
        *entry.available.lock().unwrap() = true;
    }

    fn remove(&self, entry: &Arc<PooledEngine>) {
        let mut entries = self.entries.lock().unwrap();
        entries.retain(|e| !Arc::ptr_eq(e, entry));
    }
}

/// RAII guard for engine access.
pub struct EngineGuard {
    entry: Arc<PooledEngine>,
    pool: Arc<NamePool>,
    restart: bool,
}

impl EngineGuard {
    /// Lock the engine for use.
    pub fn lock(&self) -> EngineRef<'_> {
        EngineRef {
            guard: self.entry.engine.lock().unwrap(),
        }
    }
}

impl Drop for EngineGuard {
    fn drop(&mut self) {
        if self.restart {
            self.pool.remove(&self.entry);
        } else {
            self.pool.release(&self.entry);
        }
    }
}

/// Locked reference to engine.
pub struct EngineRef<'a> {
    guard: MutexGuard<'a, UciEngine>,
}

impl<'a> Deref for EngineRef<'a> {
    type Target = UciEngine;
    fn deref(&self) -> &UciEngine {
        &*self.guard
    }
}

impl<'a> DerefMut for EngineRef<'a> {
    fn deref_mut(&mut self) -> &mut UciEngine {
        &mut *self.guard
    }
}

/// Engine cache with optional thread affinity.
pub struct EngineCache {
    affinity: bool,
    global: DashMap<String, Arc<NamePool>>,
}

impl EngineCache {
    pub fn new(affinity: bool) -> Self {
        Self {
            affinity,
            global: DashMap::new(),
        }
    }

    pub fn get_engine(
        &self,
        name: &str,
        config: &EngineConfiguration,
        realtime_logging: bool,
    ) -> Result<EngineGuard, ProcessError> {
        let pool = self.get_pool(name);
        let entry = pool.acquire(name, config, realtime_logging)?;

        Ok(EngineGuard {
            entry,
            pool,
            restart: config.restart,
        })
    }

    fn get_pool(&self, name: &str) -> Arc<NamePool> {
        if self.affinity {
            THREAD_POOLS.with(|pools| {
                pools
                    .borrow_mut()
                    .entry(name.to_string())
                    .or_default()
                    .clone()
            })
        } else {
            self.global.entry(name.to_string()).or_default().clone()
        }
    }
}

impl Default for NamePool {
    fn default() -> Self {
        Self::new()
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
    fn test_engine_cache_reuse() {
        let cache = EngineCache::new(false);
        let config = dummy_config("engine1");

        let _guard1 = cache
            .get_engine("engine1", &config, false)
            .expect("Failed to get engine");
        drop(_guard1);

        let _guard2 = cache
            .get_engine("engine1", &config, false)
            .expect("Failed to get engine");
    }

    #[test]
    fn test_engine_cache_restart_mode() {
        let cache = EngineCache::new(false);
        let config = restart_config("engine1");

        {
            let _guard = cache
                .get_engine("engine1", &config, false)
                .expect("Failed to get engine");
        }

        let _guard2 = cache
            .get_engine("engine1", &config, false)
            .expect("Failed to get engine");
    }

    #[test]
    fn test_engine_cache_two_engines_simultaneously() {
        let cache = EngineCache::new(false);
        let config1 = dummy_config("white");
        let config2 = dummy_config("black");

        let guard1 = cache
            .get_engine("white", &config1, false)
            .expect("Failed to get white engine");
        let guard2 = cache
            .get_engine("black", &config2, false)
            .expect("Failed to get black engine");

        {
            let _ref1 = guard1.lock();
            let _ref2 = guard2.lock();
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

        {
            let _guard = cache
                .get_engine("engine1", &config, false)
                .expect("Failed to get engine");
        }
    }
}
