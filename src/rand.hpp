#pragma once

#include <random>

namespace fast_chess::Random {

static std::mt19937_64 mersenne_rand;

static inline bool boolean() {
    static std::uniform_real_distribution<double> unif(0, 1);
    return unif(mersenne_rand);
}

}  // namespace fast_chess::Random