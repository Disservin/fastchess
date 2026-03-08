//! Engine cache — reuses engine processes across games.
//!
//! Uses `dashmap` for all pools (keyed by `"name"` or `"name:thread_id"` for affinity).

use dashmap::DashMap;
use std::ops::{Deref, DerefMut};
use std::sync::{Arc, Mutex, MutexGuard};
use std::thread;

use crate::engine::process::ProcessError;
use crate::engine::uci_engine::UciEngine;
use crate::types::engine_config::EngineConfiguration;

struct PooledEngine {
    engine: Mutex<UciEngine>,
    available: Mutex<bool>,
}

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
        config: &EngineConfiguration,
        realtime_logging: bool,
    ) -> Result<Arc<PooledEngine>, ProcessError> {
        let mut entries = self.entries.lock().unwrap();

        for entry in entries.iter() {
            if *entry.available.lock().unwrap() {
                *entry.available.lock().unwrap() = false;
                return Ok(Arc::clone(entry));
            }
        }

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

impl Default for NamePool {
    fn default() -> Self {
        Self::new()
    }
}

pub struct EngineGuard {
    entry: Arc<PooledEngine>,
    pool: Arc<NamePool>,
    restart: bool,
}

impl EngineGuard {
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
        let entry = pool.acquire(config, realtime_logging)?;
        Ok(EngineGuard {
            entry,
            pool,
            restart: config.restart,
        })
    }

    fn get_pool(&self, name: &str) -> Arc<NamePool> {
        let key = if self.affinity {
            format!("{}:{:?}", name, thread::current().id())
        } else {
            name.to_string()
        };
        self.global.entry(key).or_default().clone()
    }

    pub fn shutdown_handles(&self) -> Vec<EngineShutdownHandle> {
        let mut handles = Vec::new();
        for pool in self.global.iter() {
            let entries = pool.entries.lock().unwrap();
            for entry in entries.iter() {
                handles.push(EngineShutdownHandle {
                    entry: Arc::clone(entry),
                });
            }
        }
        handles
    }
}

pub struct EngineShutdownHandle {
    entry: Arc<PooledEngine>,
}

impl EngineShutdownHandle {
    pub fn terminate(&self) {
        self.entry.engine.lock().unwrap().kill();
    }

    pub fn quit(&self) {
        self.entry.engine.lock().unwrap().quit();
    }

    pub fn alive(&self) -> bool {
        self.entry.engine.lock().unwrap().alive()
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
