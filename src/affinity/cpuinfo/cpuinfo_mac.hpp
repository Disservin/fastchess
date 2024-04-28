#pragma once

#include <array>
#include <map>
#include <numeric>
#include <thread>
#include <vector>

#include <affinity/cpuinfo/cpu_info.hpp>

namespace fast_chess {
namespace affinity::cpu_info {

// Some dumb code for macOS, setting the affinity is not really supported.
inline CpuInfo getCpuInfo() noexcept {
    CpuInfo cpu_info;

    for (int i = 0; i < static_cast<int>(std::thread::hardware_concurrency()); ++i) {
        CpuInfo::PhysicalCpu::Core core = {i, {i}};

        cpu_info.physical_cpus[0].cores[i] = core;
    }

    return cpu_info;
}

}  // namespace affinity::cpu_info
}  // namespace fast_chess