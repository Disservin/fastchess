#pragma once

#include <third_party/chess.hpp>

#include <engines/engine_config.hpp>

namespace fast_chess {

struct PlayerInfo {
    EngineConfiguration config;
    chess::GameResult result = chess::GameResult::NONE;
    chess::Color color = chess::Color::NONE;
};

}  // namespace fast_chess
