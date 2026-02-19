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

use crate::log_trace;
use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

// ---------------------------------------------------------------------------
// Platform-specific implementations
// ---------------------------------------------------------------------------

#[cfg(target_os = "linux")]
mod sys {
    use crate::log_trace;

    pub fn set_thread_affinity(cpus: &[i32], pid: i32) -> bool {
        use nix::sched::{sched_setaffinity, CpuSet};
        use nix::unistd::Pid;

        log_trace!("Setting affinity mask for thread pid: {}", pid);

        let mut cpu_set = CpuSet::new();
        for &cpu in cpus {
            if cpu_set.set(cpu as usize).is_err() {
                return false;
            }
        }

        sched_setaffinity(Pid::from_raw(pid), &cpu_set).is_ok()
    }

    pub fn set_process_affinity(cpus: &[i32], pid: i32) -> bool {
        set_thread_affinity(cpus, pid)
    }

    pub fn get_process_handle() -> i32 {
        nix::unistd::getpid().as_raw()
    }

    pub fn get_thread_handle() -> i32 {
        unsafe { libc::syscall(libc::SYS_gettid) as i32 }
    }
}

#[cfg(windows)]
mod sys {
    use crate::{log_trace, log_warn};

    /// Group affinity structure for Windows processor groups.
    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct GroupAffinity {
        mask: usize,
        group: u16,
        reserved: [u16; 3],
    }

    /// Type alias for SetProcessDefaultCpuSetMasks function pointer.
    type SetProcessDefaultCpuSetMasksFn =
        unsafe extern "system" fn(
            process: windows_sys::Win32::Foundation::HANDLE,
            cpu_set_masks: *const GroupAffinity,
            cpu_set_mask_count: u16,
        ) -> windows_sys::Win32::Foundation::BOOL;

    /// Type alias for SetThreadSelectedCpuSetMasks function pointer.
    type SetThreadSelectedCpuSetMasksFn =
        unsafe extern "system" fn(
            thread: windows_sys::Win32::Foundation::HANDLE,
            cpu_set_masks: *const GroupAffinity,
            cpu_set_mask_count: u16,
        ) -> windows_sys::Win32::Foundation::BOOL;

    /// Get a function pointer from kernel32.dll.
    fn get_kernel32_proc<T>(name: &[u8]) -> Option<T> {
        use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleA, GetProcAddress};

        unsafe {
            let hmodule = GetModuleHandleA(b"kernel32.dll\0".as_ptr());
            if hmodule.is_null() {
                return None;
            }
            let proc = GetProcAddress(hmodule, name.as_ptr());
            if proc.is_none() {
                return None;
            }
            Some(std::mem::transmute_copy(&proc))
        }
    }

    /// Build GROUP_AFFINITY array from CPU list.
    fn get_group_affinities(cpus: &[i32]) -> Vec<GroupAffinity> {
        cpus.iter()
            .map(|&cpu| GroupAffinity {
                mask: 1usize << (cpu % 64),
                group: (cpu / 64) as u16,
                reserved: [0; 3],
            })
            .collect()
    }

    pub fn set_thread_affinity(cpus: &[i32], _pid: i32) -> bool {
        use windows_sys::Win32::System::Threading::{GetCurrentThread, SetThreadAffinityMask};

        log_trace!("Setting affinity mask for current thread");

        let thread_handle = unsafe { GetCurrentThread() };

        // Try Windows 11+ API first (supports >64 CPUs)
        if let Some(set_masks) =
            get_kernel32_proc::<SetThreadSelectedCpuSetMasksFn>(b"SetThreadSelectedCpuSetMasks\0")
        {
            let group_affinities = get_group_affinities(cpus);
            unsafe {
                return set_masks(
                    thread_handle,
                    group_affinities.as_ptr(),
                    group_affinities.len() as u16,
                ) != 0;
            }
        }

        // Fallback to SetThreadAffinityMask for older Windows (max 64 CPUs)
        let mut affinity_mask: usize = 0;
        for &cpu in cpus {
            if cpu > 63 {
                log_warn!(
                    "Setting thread affinity for more than 64 logical CPUs is not supported: \
                     requires at least Windows 11 or Windows Server 2022."
                );
                return false;
            }
            affinity_mask |= 1usize << cpu;
        }

        unsafe { SetThreadAffinityMask(thread_handle, affinity_mask) != 0 }
    }

    pub fn set_process_affinity(cpus: &[i32], pid: i32) -> bool {
        use windows_sys::Win32::System::Threading::{
            GetCurrentProcess, OpenProcess, SetProcessAffinityMask, PROCESS_SET_INFORMATION,
        };

        log_trace!("Setting affinity mask for process pid: {}", pid);

        let process_handle = if pid == 0
            || pid == unsafe { windows_sys::Win32::System::Threading::GetCurrentProcessId() as i32 }
        {
            unsafe { GetCurrentProcess() }
        } else {
            let handle = unsafe { OpenProcess(PROCESS_SET_INFORMATION, 0, pid as u32) };
            if handle.is_null() {
                return false;
            }
            handle
        };

        // Try Windows 11+ API first (supports >64 CPUs)
        if let Some(set_masks) =
            get_kernel32_proc::<SetProcessDefaultCpuSetMasksFn>(b"SetProcessDefaultCpuSetMasks\0")
        {
            let group_affinities = get_group_affinities(cpus);
            let result = unsafe {
                set_masks(
                    process_handle,
                    group_affinities.as_ptr(),
                    group_affinities.len() as u16,
                ) != 0
            };
            // Close handle if we opened it
            if process_handle != unsafe { GetCurrentProcess() } {
                unsafe { windows_sys::Win32::Foundation::CloseHandle(process_handle) };
            }
            return result;
        }

        // Fallback to SetProcessAffinityMask for older Windows (max 64 CPUs)
        let mut affinity_mask: usize = 0;
        for &cpu in cpus {
            if cpu > 63 {
                log_warn!(
                    "Setting process affinity for more than 64 logical CPUs is not supported: \
                     requires at least Windows 11 or Windows Server 2022."
                );
                return false;
            }
            affinity_mask |= 1usize << cpu;
        }

        let result = unsafe { SetProcessAffinityMask(process_handle, affinity_mask) != 0 };

        // Close handle if we opened it
        if process_handle != unsafe { GetCurrentProcess() } {
            unsafe { windows_sys::Win32::Foundation::CloseHandle(process_handle) };
        }

        result
    }

    /// Set affinity for a process using a raw Windows handle.
    /// This is used by the engine process module which already has a handle.
    pub fn set_process_affinity_for_handle(
        cpus: &[i32],
        process_handle: windows_sys::Win32::Foundation::HANDLE,
    ) -> bool {
        use windows_sys::Win32::System::Threading::SetProcessAffinityMask;

        log_trace!(
            "Setting affinity mask for process handle: {:?}",
            process_handle
        );

        // Try Windows 11+ API first (supports >64 CPUs)
        if let Some(set_masks) =
            get_kernel32_proc::<SetProcessDefaultCpuSetMasksFn>(b"SetProcessDefaultCpuSetMasks\0")
        {
            let group_affinities = get_group_affinities(cpus);
            unsafe {
                return set_masks(
                    process_handle,
                    group_affinities.as_ptr(),
                    group_affinities.len() as u16,
                ) != 0;
            }
        }

        // Fallback to SetProcessAffinityMask for older Windows (max 64 CPUs)
        let mut affinity_mask: usize = 0;
        for &cpu in cpus {
            if cpu > 63 {
                log_warn!(
                    "Setting process affinity for more than 64 logical CPUs is not supported: \
                     requires at least Windows 11 or Windows Server 2022."
                );
                return false;
            }
            affinity_mask |= 1usize << cpu;
        }

        unsafe { SetProcessAffinityMask(process_handle, affinity_mask) != 0 }
    }

    pub fn get_process_handle() -> i32 {
        unsafe { windows_sys::Win32::System::Threading::GetCurrentProcessId() as i32 }
    }

    pub fn get_thread_handle() -> i32 {
        unsafe { windows_sys::Win32::System::Threading::GetCurrentThreadId() as i32 }
    }
}

#[cfg(not(any(target_os = "linux", windows)))]
mod sys {
    pub fn set_thread_affinity(_cpus: &[i32], _pid: i32) -> bool {
        false
    }

    pub fn set_process_affinity(_cpus: &[i32], _pid: i32) -> bool {
        false
    }

    pub fn get_process_handle() -> i32 {
        0
    }

    pub fn get_thread_handle() -> i32 {
        0
    }
}

// Re-export platform-specific functions
pub use sys::*;

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
            log_trace!("Using affinity");
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
        log_trace!("Setting up cores");

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
        log_trace!("Setting up selected cores");

        for &cpu in cpus {
            self.cores[HtGroup::Ht1 as usize].push_back(AffinityProcessor::new(vec![cpu]));
        }
    }
}

// AffinityManager is Send+Sync because all interior mutation goes through
// the mutex or atomics.
// unsafe impl Send for AffinityManager {}
// unsafe impl Sync for AffinityManager {}

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
