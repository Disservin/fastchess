#pragma once

#include <cstdint>
#include <limits>
#include <random>

#include <core/logger/logger.hpp>

namespace fastchess::random {
inline std::mt19937_64 mersenne_rand;

inline uint64_t random_uint64() {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) | rd());
    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());

    return dist(gen);
}

inline void seed(uint64_t seed) {
    LOG_INFO("Setting seed to: {}", seed);
    mersenne_rand.seed(seed);
}
}  // namespace fastchess::random