#pragma once

#include <vector>
#include <array>
#include <thread>
#include <numeric>
#include <map>

namespace affinity {

/// @brief Some dumb code for macOS, setting the affinity is not supported.
/// @return
inline std::map<int, std::array<std::vector<int>, 2>> getPhysicalCores() noexcept {
    std::vector<int> ht_1;

    ht_1.resize(std::thread::hardware_concurrency());

    // Just put all cores in ht_1
    std::iota(ht_1.begin(), ht_1.end(), 0);

    // limited to one physical cpui
    return {{0, {ht_1, std::vector<int>()}}};
}

}  // namespace affinity