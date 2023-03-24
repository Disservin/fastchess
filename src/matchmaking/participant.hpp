#pragma once

#include "engines/uci_engine.hpp"

namespace fast_chess {

struct PlayerInfo {
    std::string termination;
    GameResult score = GameResult::NONE;
    Color color = NO_COLOR;
    EngineConfiguration config;
};

inline bool operator==(const PlayerInfo &lhs, const EngineConfiguration &rhs) {
    return lhs.config.name == rhs.name;
}

inline bool operator!=(const PlayerInfo &lhs, const EngineConfiguration &rhs) {
    return !(lhs == rhs);
}

class Participant : public UciEngine {
   public:
    explicit Participant(const EngineConfiguration &config) {
        loadConfig(config);
        startEngine(config.cmd);

        info_.config = config;
    }

    PlayerInfo info_;
};

}  // namespace fast_chess