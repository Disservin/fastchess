use dashmap::DashMap;
use std::cell::RefCell;
use std::collections::HashMap;
use std::sync::{Arc, Mutex};

use crate::engine::process::ProcessError;
use crate::engine::uci_engine::UciEngine;
use crate::types::engine_config::EngineConfiguration;

thread_local! {
    static THREAD_POOLS: RefCell<HashMap<String, Arc<NamePool>>> =
        RefCell::new(HashMap::new());
}

struct PooledEngine {
    engine: UciEngine,
    available: bool,
}

struct NamePool {
    entries: Mutex<Vec<PooledEngine>>,
}

impl NamePool {
    fn new() -> Self {
        Self {
            entries: Mutex::new(Vec::new()),
        }
    }

    fn with_engine<F, R>(
        &self,
        config: &EngineConfiguration,
        realtime_logging: bool,
        f: F,
    ) -> Result<R, ProcessError>
    where
        F: FnOnce(&mut UciEngine) -> R,
    {
        let idx = {
            let mut entries = self.entries.lock().unwrap();
            if let Some(idx) = entries.iter().position(|e| e.available) {
                entries[idx].available = false;
                idx
            } else {
                let engine = UciEngine::new(config, realtime_logging)?;
                entries.push(PooledEngine {
                    engine,
                    available: false,
                });
                entries.len() - 1
            }
        };

        let result = {
            let mut entries = self.entries.lock().unwrap();
            f(&mut entries[idx].engine)
        };

        if config.restart {
            let mut entries = self.entries.lock().unwrap();
            entries.remove(idx);
        } else {
            let mut entries = self.entries.lock().unwrap();
            entries[idx].available = true;
        }

        Ok(result)
    }

    fn with_engines<F, R>(
        &self,
        config: &EngineConfiguration,
        realtime_logging: bool,
        other: &NamePool,
        other_config: &EngineConfiguration,
        f: F,
    ) -> Result<R, ProcessError>
    where
        F: FnOnce(&mut UciEngine, &mut UciEngine) -> R,
    {
        self.with_engine(config, realtime_logging, |a| {
            other.with_engine(other_config, realtime_logging, |b| f(a, b))
        })
        .flatten()
    }
}

impl Default for NamePool {
    fn default() -> Self {
        Self::new()
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

    pub fn with_engine<F, R>(
        &self,
        name: &str,
        config: &EngineConfiguration,
        realtime_logging: bool,
        f: F,
    ) -> Result<R, ProcessError>
    where
        F: FnOnce(&mut UciEngine) -> R,
    {
        self.get_pool(name).with_engine(config, realtime_logging, f)
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

    pub fn with_engines<F, R>(
        &self,
        white_name: &str,
        white_config: &EngineConfiguration,
        black_name: &str,
        black_config: &EngineConfiguration,
        realtime_logging: bool,
        f: F,
    ) -> Result<R, ProcessError>
    where
        F: FnOnce(&mut UciEngine, &mut UciEngine) -> R,
    {
        let white_pool = self.get_pool(white_name);
        let black_pool = self.get_pool(black_name);
        white_pool.with_engines(white_config, realtime_logging, &black_pool, black_config, f)
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

        cache
            .with_engine("engine1", &config, false, |_e| {})
            .expect("Failed to get engine");

        cache
            .with_engine("engine1", &config, false, |_e| {})
            .expect("Failed to get engine on reuse");
    }

    #[test]
    fn test_engine_cache_restart_mode() {
        let cache = EngineCache::new(false);
        let config = restart_config("engine1");

        cache
            .with_engine("engine1", &config, false, |_e| {})
            .expect("Failed to get engine");

        cache
            .with_engine("engine1", &config, false, |_e| {})
            .expect("Failed to get engine after restart");
    }

    #[test]
    fn test_engine_cache_two_engines_simultaneously() {
        let cache = EngineCache::new(false);
        let config1 = dummy_config("white");
        let config2 = dummy_config("black");

        cache
            .with_engines("white", &config1, "black", &config2, false, |_w, _b| {})
            .expect("Failed to get both engines");
    }

    #[test]
    fn test_engine_cache_affinity_mode() {
        let cache = EngineCache::new(true);
        let config = dummy_config("engine1");

        cache
            .with_engine("engine1", &config, false, |_e| {})
            .expect("Failed to get engine");

        cache
            .with_engine("engine1", &config, false, |_e| {})
            .expect("Failed to get engine on second access");
    }
}
