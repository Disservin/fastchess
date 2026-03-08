//! CPU topology detection.
//!
//! Parses `/proc/cpuinfo` on Linux to enumerate physical packages, cores,
//! and logical processors (hyperthreads). On macOS and other unsupported
//! platforms, a flat topology is synthesised from `num_cpus`.

use crate::{log_trace, log_warn};
use std::collections::BTreeMap;

/// A single logical processor (hyperthread).
#[derive(Debug, Clone)]
pub struct Processor {
    pub processor_id: i32,
}

/// A physical core containing one or more logical processors.
#[derive(Debug, Clone)]
pub struct Core {
    pub core_id: i32,
    pub processors: Vec<Processor>,
}

/// A physical CPU package containing one or more cores.
#[derive(Debug, Clone)]
pub struct PhysicalCpu {
    pub physical_id: i32,
    pub cores: BTreeMap<i32, Core>,
}

/// Complete CPU topology of the system.
#[derive(Debug, Clone)]
pub struct CpuInfo {
    pub physical_cpus: BTreeMap<i32, PhysicalCpu>,
}

/// Parse `/proc/cpuinfo` (Linux) to build the CPU topology.
#[cfg(target_os = "linux")]
pub fn get_cpu_info() -> CpuInfo {
    use std::fs;

    log_trace!("Getting CPU info");

    let mut cpu_info = CpuInfo {
        physical_cpus: BTreeMap::new(),
    };

    let contents = match fs::read_to_string("/proc/cpuinfo") {
        Ok(c) => c,
        Err(e) => {
            log_warn!("Failed to read /proc/cpuinfo: {}", e);
            return cpu_info;
        }
    };

    let mut processor_id: Option<i32> = None;
    let mut core_id: Option<i32> = None;
    let mut physical_id: Option<i32> = None;

    let extract_value = |line: &str| -> Option<i32> {
        let colon_pos = line.find(':')?;
        line[colon_pos + 1..].trim().parse::<i32>().ok()
    };

    for line in contents.lines() {
        if line.starts_with("processor") {
            processor_id = extract_value(line);
        } else if line.starts_with("core id") {
            core_id = extract_value(line);
        } else if line.starts_with("physical id") {
            physical_id = extract_value(line);
        }

        if let (Some(pid), Some(cid), Some(phid)) = (processor_id, core_id, physical_id) {
            let physical_cpu = cpu_info
                .physical_cpus
                .entry(phid)
                .or_insert_with(|| PhysicalCpu {
                    physical_id: phid,
                    cores: BTreeMap::new(),
                });

            let core = physical_cpu.cores.entry(cid).or_insert_with(|| Core {
                core_id: cid,
                processors: Vec::new(),
            });

            core.processors.push(Processor { processor_id: pid });

            processor_id = None;
            core_id = None;
            physical_id = None;
        }
    }

    cpu_info
}

/// On macOS, CPU topology detection is simplified: each logical processor
/// is treated as its own core under a single physical package.
#[cfg(target_os = "macos")]
pub fn get_cpu_info() -> CpuInfo {
    let count = std::thread::available_parallelism()
        .map(|n| n.get() as i32)
        .unwrap_or(1);

    let mut cores = BTreeMap::new();
    for i in 0..count {
        cores.insert(
            i,
            Core {
                core_id: i,
                processors: vec![Processor { processor_id: i }],
            },
        );
    }

    let mut physical_cpus = BTreeMap::new();
    physical_cpus.insert(
        0,
        PhysicalCpu {
            physical_id: 0,
            cores,
        },
    );

    CpuInfo { physical_cpus }
}

/// Fallback for unsupported platforms: same simplified topology as macOS.
#[cfg(not(any(target_os = "linux", target_os = "macos")))]
pub fn get_cpu_info() -> CpuInfo {
    let count = std::thread::available_parallelism()
        .map(|n| n.get() as i32)
        .unwrap_or(1);

    let mut cores = BTreeMap::new();
    for i in 0..count {
        cores.insert(
            i,
            Core {
                core_id: i,
                processors: vec![Processor { processor_id: i }],
            },
        );
    }

    let mut physical_cpus = BTreeMap::new();
    physical_cpus.insert(
        0,
        PhysicalCpu {
            physical_id: 0,
            cores,
        },
    );

    CpuInfo { physical_cpus }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cpu_info_not_empty() {
        let info = get_cpu_info();
        // On any real system we should have at least one physical CPU.
        assert!(!info.physical_cpus.is_empty());
        // And at least one core with at least one processor.
        let first_cpu = info.physical_cpus.values().next().unwrap();
        assert!(!first_cpu.cores.is_empty());
        let first_core = first_cpu.cores.values().next().unwrap();
        assert!(!first_core.processors.is_empty());
    }

    #[test]
    fn test_processor_ids_are_unique() {
        let info = get_cpu_info();
        let mut ids: Vec<i32> = Vec::new();
        for cpu in info.physical_cpus.values() {
            for core in cpu.cores.values() {
                for proc in &core.processors {
                    assert!(
                        !ids.contains(&proc.processor_id),
                        "Duplicate processor id: {}",
                        proc.processor_id
                    );
                    ids.push(proc.processor_id);
                }
            }
        }
    }
}
