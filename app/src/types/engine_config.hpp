#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <core/helper.hpp>
#include <game/timecontrol/timecontrol.hpp>
#include <types/enums.hpp>

#include <json.hpp>

namespace fastchess {

struct Limit {
    TimeControl::Limits tc;

    uint64_t nodes = 0;
    uint64_t plies = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Limit, tc, nodes, plies)

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

    // Should process be restarted after every game
    bool restart = false;

    // UCI options
    std::unordered_map<std::string, std::string> options;

    // Chess variant
    VariantType variant = VariantType::STANDARD;

    template <typename T, typename Predicate>
    std::optional<T> getOption(std::string_view option_name, Predicate transform) const {
        const auto it = options.find(option_name.data());

        if (it != options.end()) {
            return std::optional<T>(transform(it->second));
        }

        return std::nullopt;
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EngineConfiguration, name, dir, cmd, args, restart, options, limit, variant)

}  // namespace fastchess
