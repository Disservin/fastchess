#pragma once

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <time/timecontrol.hpp>
#include <types/enums.hpp>
#include <util/helper.hpp>

#include <json.hpp>

namespace fastchess {

struct Limit {
    TimeControl::Limits tc;

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

    // Recover engine after crash
    bool recover = false;

    // Trust an engine's mate score
    bool trust = false

    template <typename T, typename Predicate>
    std::optional<T> getOption(std::string_view option_name, Predicate transform) const {
        const auto it = std::find_if(options.begin(), options.end(),
                                     [&option_name](const auto &option) { return option.first == option_name; });

        if (it != options.end()) {
            return std::optional<T>(transform(it->second));
        }

        return std::nullopt;
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(EngineConfiguration, name, dir, cmd, args, options, limit, variant,
                                                recover, trust)

}  // namespace fastchess
