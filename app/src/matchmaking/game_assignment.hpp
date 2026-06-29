#pragma once

#include <utility>

#include <types/engine_config.hpp>

namespace fastchess {

struct GameAssignment {
    EngineConfiguration first;
    EngineConfiguration second;

    GameAssignment() = default;
    GameAssignment(EngineConfiguration first, EngineConfiguration second)
        : first(std::move(first)), second(std::move(second)) {}

    [[nodiscard]] const std::string& first_name() const noexcept { return first.name; }
    [[nodiscard]] const std::string& second_name() const noexcept { return second.name; }
};

}  // namespace fastchess
