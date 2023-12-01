#pragma once

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <types/enums.hpp>
#include <util/helper.hpp>

#include <json.hpp>

namespace fast_chess {

/// @brief @todo use std::chrono::milliseconds
struct TimeControl {
    // go winc/binc, in milliseconds
    uint64_t increment = 0;
    // go movetime, in milliseconds
    int64_t fixed_time = 0;
    // go wtime/btime, in milliseconds
    int64_t time = 0;
    // go movestogo
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
    // the limit for the engines "go" command
    Limit limit;

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

    // Chess variant
    VariantType variant = VariantType::STANDARD;

    bool recover = false;

    template <typename T, typename Predicate>
    std::optional<T> getOption(std::string_view option_name, Predicate transform) const {
        const auto it = std::find_if(
            options.begin(), options.end(),
            [&option_name](const auto &option) { return option.first == option_name; });

        if (it != options.end()) {
            return std::optional<T>(transform(it->second));
        }

        return std::nullopt;
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(EngineConfiguration, name, dir, cmd, args, options,
                                                limit, variant, recover)

}  // namespace fast_chess
