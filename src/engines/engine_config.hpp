#pragma once

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "third_party/json.hpp"

namespace fast_chess {

/*
Modified version of the NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE macro in nlohmann's json lib.
ordered_json type conversion is not yet supported, though we only have to change the type.
*/
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Type, ...)                                \
    inline void to_json(nlohmann::ordered_json &nlohmann_json_j, const Type &nlohmann_json_t) {   \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__))                  \
    }                                                                                             \
    inline void from_json(const nlohmann::ordered_json &nlohmann_json_j, Type &nlohmann_json_t) { \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__))                \
    }

struct TimeControl {
    uint64_t increment = 0;
    int64_t fixed_time = 0;
    int64_t time = 0;
    int moves = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(TimeControl, increment, fixed_time, time, moves)

inline std::ostream &operator<<(std::ostream &os, const TimeControl &tc) {
    if (tc.moves > 0) os << tc.moves << "/";

    os << (tc.time / 1000.0);

    if (tc.increment > 0) os << "+" << (tc.increment / 1000.0);

    return os;
}

struct Limit {
    TimeControl tc;

    uint64_t nodes = 0;
    uint64_t plies = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Limit, tc, nodes, plies)

struct EngineConfiguration {
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

    Limit limit;

    bool recover = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(EngineConfiguration, name, dir, cmd, args, options,
                                                limit, recover)

}  // namespace fast_chess
