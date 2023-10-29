#pragma once

#include <vector>
#include <array>
#include <thread>

namespace fast_chess {

inline std::array<std::vector<int>, 2> get_physical_cores() {
    std::vector<int> ht_1;
    std::vector<int> ht_2;

    ht_1.reserve(std::thread::hardware_concurrency() / 2);
    ht_2.reserve(std::thread::hardware_concurrency() / 2);

    /*
    processor       : 0
    core id         : 0
    processor       : 1
    core id         : 0
    processor       : 2
    core id         : 1
    processor       : 3
    core id         : 1
    processor       : 4
    core id         : 2
    processor       : 5
    core id         : 2
    */
    for (unsigned int i = 0; i < std::thread::hardware_concurrency(); i++) {
        if (i % 2 == 0) {
            ht_1.push_back(i);
        } else {
            ht_2.push_back(i);
        }
    }

    return {ht_1, ht_2};
}
}  // namespace fast_chess