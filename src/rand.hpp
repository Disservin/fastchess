#pragma once

#include <random>

namespace fast_chess::Random {

static std::mt19937_64 mersenne_rand;

static inline bool boolean() {
    static auto gen =
        std::bind(std::uniform_int_distribution<>(0, 1), std::default_random_engine());

    return gen();
}

}  // namespace fast_chess::Random