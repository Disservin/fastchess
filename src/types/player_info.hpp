#pragma once

#include <chess.hpp>

#include <types/engine_config.hpp>

namespace fast_chess {

struct PlayerInfo {
    EngineConfiguration config;
    chess::GameResult result = chess::GameResult::NONE;
    chess::Color color       = chess::Color::NONE;
};

}  // namespace fast_chess
