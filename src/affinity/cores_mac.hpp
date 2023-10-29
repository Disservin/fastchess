#pragma once

#include <vector>
#include <array>
#include <thread>
#include <numeric>

namespace affinity {

/// @brief Some dumb code for macOS, setting the affinity is not supported.
/// @return
inline std::array<std::vector<int>, 2> get_physical_cores() noexcept {
    std::vector<int> ht_1;

    ht_1.resize(std::thread::hardware_concurrency());

    // Just put all cores in ht_1
    std::iota(ht_1.begin(), ht_1.end(), 0);

    return {ht_1, std::vector<int>()};
}

}  // namespace affinity