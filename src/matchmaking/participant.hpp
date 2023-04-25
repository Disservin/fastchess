#pragma once

#include "../engines/engine_config.hpp"
#include "../engines/uci_engine.hpp"
#include "../third_party/chess.hpp"

namespace fast_chess {

struct PlayerInfo {
    EngineConfiguration config;

    std::string termination;
    Chess::GameResult score = Chess::GameResult::NONE;
    Chess::Color color = Chess::Color::NO_COLOR;
};

inline bool operator==(const PlayerInfo &lhs, const EngineConfiguration &rhs) {
    return lhs.config.name == rhs.name;
}

inline bool operator!=(const PlayerInfo &lhs, const EngineConfiguration &rhs) {
    return !(lhs == rhs);
}

struct Participant {
    UciEngine engine;
    PlayerInfo info;
};

}  // namespace fast_chess
