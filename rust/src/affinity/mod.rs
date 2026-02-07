//! CPU affinity management.
//!
//! Provides:
//! - Low-level functions to set thread/process CPU affinity (`set_thread_affinity`,
//!   `set_process_affinity`, `get_thread_handle`, `get_process_handle`).
//! - `AffinityManager` — manages a pool of CPU cores split by hyperthread group,
//!   handing them out to threadpool workers via `consume()`.
//! - `CpuGuard` — RAII guard that releases the CPU core back to the pool on drop.
//!
//! Ports `affinity/affinity.hpp` and `affinity/affinity_manager.hpp` from C++.

pub mod cpu_info;

use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

// ---------------------------------------------------------------------------
// Low-level affinity functions (Linux only, stubs elsewhere)
// ---------------------------------------------------------------------------

/// Set CPU affinity for the given thread/pid.
///
/// On Linux, uses `sched_setaffinity`. On other platforms, returns `false`.
#[cfg(target_os = "linux")]
pub fn set_thread_affinity(cpus: &[i32], pid: i32) -> bool {
    use nix::sched::{sched_setaffinity, CpuSet};
    use nix::unistd::Pid;

    log::trace!("Setting affinity mask for thread pid: {}", pid);

    let mut cpu_set = CpuSet::new();
    for &cpu in cpus {
        if cpu_set.set(cpu as usize).is_err() {
            return false;
        }
    }

    sched_setaffinity(Pid::from_raw(pid), &cpu_set).is_ok()
}

#[cfg(not(target_os = "linux"))]
pub fn set_thread_affinity(_cpus: &[i32], _pid: i32) -> bool {
    false
}

/// Set CPU affinity for a process (delegates to `set_thread_affinity` on Linux).
pub fn set_process_affinity(cpus: &[i32], pid: i32) -> bool {
    set_thread_affinity(cpus, pid)
}

/// Return the current process PID (for use with `set_process_affinity`).
pub fn get_process_handle() -> i32 {
    #[cfg(unix)]
    {
        nix::unistd::getpid().as_raw()
    }
    #[cfg(not(unix))]
    {
        0
    }
}

/// Return the current thread ID (for use with `set_thread_affinity`).
pub fn get_thread_handle() -> i32 {
    #[cfg(target_os = "linux")]
    {
        unsafe { libc::syscall(libc::SYS_gettid) as i32 }
    }
    #[cfg(not(target_os = "linux"))]
    {
        0
    }
}

// ---------------------------------------------------------------------------
// AffinityManager
// ---------------------------------------------------------------------------

/// Hyperthread group index.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum HtGroup {
    Ht1 = 0,
    Ht2 = 1,
}

/// A processor entry in the affinity pool.
///
/// This is the Rust equivalent of C++ `AffinityProcessor : ScopeEntry`.
/// The `available` flag is atomic so it can be checked from any thread.
#[derive(Debug)]
struct AffinityProcessor {
    cpus: Vec<i32>,
    available: AtomicBool,
}

impl AffinityProcessor {
    fn new(cpus: Vec<i32>) -> Self {
        Self {
            cpus,
            available: AtomicBool::new(true),
        }
    }

    fn release(&self) {
        self.available.store(true, Ordering::Release);
    }
}

/// RAII guard returned by `AffinityManager::consume()`.
///
/// When dropped, the CPU core is released back into the pool.
/// Access the assigned CPUs via the `cpus()` method.
pub struct CpuGuard<'a> {
    processor: Option<&'a AffinityProcessor>,
}

impl<'a> CpuGuard<'a> {
    /// The CPUs assigned to this guard. Empty if affinity is disabled.
    pub fn cpus(&self) -> &[i32] {
        match self.processor {
            Some(p) => &p.cpus,
            None => &[],
        }
    }
}

impl<'a> Drop for CpuGuard<'a> {
    fn drop(&mut self) {
        if let Some(p) = self.processor.take() {
            p.release();
        }
    }
}

/// Manages a pool of CPU cores, split into two hyperthread groups.
///
/// Cores from `HT_1` are used first; once exhausted, `HT_2` is used.
/// This ensures that when possible, two engine processes are not placed
/// on the same physical core (which would share execution resources).
pub struct AffinityManager {
    /// Two deques of processors, indexed by `HtGroup`.
    cores: [VecDeque<AffinityProcessor>; 2],
    /// Mutex protecting `consume`.
    mutex: Mutex<()>,
    /// Whether affinity is enabled.
    use_affinity: bool,
}

impl AffinityManager {
    /// Create a new `AffinityManager`.
    ///
    /// - `use_affinity`: master toggle from config.
    /// - `cpus`: user-specified CPU list (empty → auto-detect).
    /// - `tpe`: threads-per-engine; if >1, affinity is disabled.
    pub fn new(use_affinity: bool, cpus: &[i32], tpe: i32) -> Self {
        let mut mgr = Self {
            cores: [VecDeque::new(), VecDeque::new()],
            mutex: Mutex::new(()),
            use_affinity: use_affinity && tpe <= 1,
        };

        if mgr.use_affinity {
            if cpus.is_empty() {
                mgr.setup_cores(&cpu_info::get_cpu_info());
            } else {
                mgr.setup_selected_cores(cpus);
            }
            log::trace!("Using affinity");
        }

        mgr
    }

    /// Whether affinity is active.
    pub fn is_active(&self) -> bool {
        self.use_affinity
    }

    /// Acquire the next available CPU core from the pool.
    ///
    /// Returns a `CpuGuard` that holds the core reservation and releases it
    /// when dropped. If affinity is disabled, returns a guard with empty CPUs.
    ///
    /// # Errors
    ///
    /// Returns `Err` if all cores are currently in use.
    pub fn consume(&self) -> Result<CpuGuard<'_>, String> {
        if !self.use_affinity {
            return Ok(CpuGuard { processor: None });
        }

        let _lock = self.mutex.lock().unwrap();

        // Check both HT groups
        for group in [HtGroup::Ht1, HtGroup::Ht2] {
            for proc in &self.cores[group as usize] {
                if proc.available.load(Ordering::Acquire) {
                    proc.available.store(false, Ordering::Release);
                    return Ok(CpuGuard {
                        processor: Some(proc),
                    });
                }
            }
        }

        Err("No cores available, all are in use".to_string())
    }

    /// Populate the core pool from auto-detected CPU topology.
    ///
    /// Processors from the same core are alternated across HT groups,
    /// so that HT_1 contains one hyperthread per physical core and
    /// HT_2 contains the other.
    fn setup_cores(&mut self, cpu_info: &cpu_info::CpuInfo) {
        log::trace!("Setting up cores");

        for physical_cpu in cpu_info.physical_cpus.values() {
            for core in physical_cpu.cores.values() {
                for (idx, processor) in core.processors.iter().enumerate() {
                    let group = if idx % 2 == 0 {
                        HtGroup::Ht1
                    } else {
                        HtGroup::Ht2
                    };
                    self.cores[group as usize]
                        .push_back(AffinityProcessor::new(vec![processor.processor_id]));
                }
            }
        }
    }

    /// Populate the core pool from a user-specified list of CPUs.
    /// All entries go into HT_1.
    fn setup_selected_cores(&mut self, cpus: &[i32]) {
        log::trace!("Setting up selected cores");

        for &cpu in cpus {
            self.cores[HtGroup::Ht1 as usize].push_back(AffinityProcessor::new(vec![cpu]));
        }
    }
}

// AffinityManager is Send+Sync because all interior mutation goes through
// the mutex or atomics.
unsafe impl Send for AffinityManager {}
unsafe impl Sync for AffinityManager {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_disabled_affinity() {
        let mgr = AffinityManager::new(false, &[], 1);
        assert!(!mgr.is_active());
        let guard = mgr.consume().unwrap();
        assert!(guard.cpus().is_empty());
    }

    #[test]
    fn test_selected_cores() {
        let mgr = AffinityManager::new(true, &[0, 2, 4], 1);
        assert!(mgr.is_active());

        let g1 = mgr.consume().unwrap();
        assert_eq!(g1.cpus(), &[0]);

        let g2 = mgr.consume().unwrap();
        assert_eq!(g2.cpus(), &[2]);

        let g3 = mgr.consume().unwrap();
        assert_eq!(g3.cpus(), &[4]);

        // All consumed — should fail
        assert!(mgr.consume().is_err());

        // Drop g1 to release core 0
        drop(g1);

        let g4 = mgr.consume().unwrap();
        assert_eq!(g4.cpus(), &[0]);

        drop(g2);
        drop(g3);
        drop(g4);
    }

    #[test]
    fn test_tpe_disables_affinity() {
        // threads_per_engine > 1 should disable affinity
        let mgr = AffinityManager::new(true, &[0, 1, 2, 3], 2);
        assert!(!mgr.is_active());
        let guard = mgr.consume().unwrap();
        assert!(guard.cpus().is_empty());
    }

    #[test]
    fn test_auto_detect_cores() {
        // Test with auto-detected CPU info (should work on any system)
        let mgr = AffinityManager::new(true, &[], 1);
        assert!(mgr.is_active());
        // Should be able to consume at least one core on any system
        let guard = mgr.consume().unwrap();
        assert!(!guard.cpus().is_empty());
        drop(guard);
    }

    #[test]
    fn test_ht_group_ordering() {
        // Build a fake CpuInfo with 2 cores, each with 2 hyperthreads
        let mut info = cpu_info::CpuInfo {
            physical_cpus: std::collections::BTreeMap::new(),
        };

        let mut cores = std::collections::BTreeMap::new();
        cores.insert(
            0,
            cpu_info::Core {
                core_id: 0,
                processors: vec![
                    cpu_info::Processor { processor_id: 0 },
                    cpu_info::Processor { processor_id: 2 },
                ],
            },
        );
        cores.insert(
            1,
            cpu_info::Core {
                core_id: 1,
                processors: vec![
                    cpu_info::Processor { processor_id: 1 },
                    cpu_info::Processor { processor_id: 3 },
                ],
            },
        );

        info.physical_cpus.insert(
            0,
            cpu_info::PhysicalCpu {
                physical_id: 0,
                cores,
            },
        );

        let mut mgr = AffinityManager {
            cores: [VecDeque::new(), VecDeque::new()],
            mutex: Mutex::new(()),
            use_affinity: true,
        };
        mgr.setup_cores(&info);

        // HT_1 should have processors 0 and 1 (first hyperthread of each core)
        // HT_2 should have processors 2 and 3 (second hyperthread of each core)
        let g1 = mgr.consume().unwrap();
        assert_eq!(g1.cpus(), &[0]); // HT_1, core 0

        let g2 = mgr.consume().unwrap();
        assert_eq!(g2.cpus(), &[1]); // HT_1, core 1

        let g3 = mgr.consume().unwrap();
        assert_eq!(g3.cpus(), &[2]); // HT_2, core 0

        let g4 = mgr.consume().unwrap();
        assert_eq!(g4.cpus(), &[3]); // HT_2, core 1

        assert!(mgr.consume().is_err());

        drop(g1);
        drop(g2);
        drop(g3);
        drop(g4);
    }
}
