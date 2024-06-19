#pragma once

#include <random>

namespace fast_chess::util::random {
inline std::mt19937_64 mersenne_rand;
inline std::random_device random_device;

// Function to generate a random integer between x and y (inclusive)
inline uint64_t rand_int(uint64_t x, uint64_t y) {
    std::uniform_int_distribution<uint64_t> dist(x, y);
    return dist(mersenne_rand);
}

// Function to generate a random floating-point number between x and y (inclusive)
inline double rand_double(double x, double y) {
    std::uniform_real_distribution<double> dist(x, y);
    return dist(mersenne_rand);
}
}  // namespace fast_chess::util::random
