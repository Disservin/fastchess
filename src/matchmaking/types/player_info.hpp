#pragma once

#include <third_party/chess.hpp>

#include <engines/engine_config.hpp>

namespace fast_chess {

struct PlayerInfo {
    EngineConfiguration config;
    Chess::GameResult result = Chess::GameResult::NONE;
    Chess::Color color = Chess::Color::NO_COLOR;
};

}  // namespace fast_chess
