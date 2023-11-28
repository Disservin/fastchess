#pragma once

#include <vector>
#include <array>
#include <thread>
#include <numeric>
#include <map>

#include <affinity/cpu_info.hpp>

namespace affinity {

/// @brief Some dumb code for macOS, setting the affinity is not supported.
/// @return
inline CpuInfo getCpuInfo() noexcept {
    std::vector<CpuInfo::PhysicalCpu::Core::Processor> procs;

    procs.resize(std::thread::hardware_concurrency(), {0});

    // Just put all cores in ht_1
    std::iota(procs.begin(), procs.end(), 0);

    // limited to one physical cpui
    CpuInfo cpu_info;
    cpu_info.physical_cpus[0].cores[0].processors = procs;

    return cpu_info;
}

}  // namespace affinity