#pragma once
#include <random>

namespace Random
{
static std::random_device rd;
static std::mt19937 generator(rd());
} // namespace Random