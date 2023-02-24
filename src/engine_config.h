#pragma once

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

struct TimeControl
{
    int moves = 0;
    int64_t time = 0;
    uint64_t increment = 0;
};

inline std::ostream &operator<<(std::ostream &os, const TimeControl &tc)
{
    if (tc.moves > 0)
        os << tc.moves << "/";

    os << tc.time;

    if (tc.increment > 0)
        os << "+" << tc.increment;

    return os;
}

struct EngineConfiguration
{
    // engine name
    std::string name;

    // Path to engine
    std::string dir;

    // Engine binary name
    std::string cmd;

    // Custom args that should be sent
    std::string args;

    // UCI options
    std::vector<std::pair<std::string, std::string>> options;

    // time control for the engine
    TimeControl tc;

    // Node limit for the engine
    uint64_t nodes = 0;

    // Ply limit for the engine
    uint64_t plies = 0;
};