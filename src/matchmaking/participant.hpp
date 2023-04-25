#pragma once

#include "../engines/engine_config.hpp"
#include "../engines/uci_engine.hpp"
#include "../third_party/chess.hpp"

namespace fast_chess {

struct PlayerInfo {
    EngineConfiguration config;

    Chess::GameResult result = Chess::GameResult::NONE;
    Chess::Color color = Chess::Color::NO_COLOR;
};

inline bool operator==(const PlayerInfo &lhs, const EngineConfiguration &rhs) {
    return lhs.config.name == rhs.name;
}

inline bool operator!=(const PlayerInfo &lhs, const EngineConfiguration &rhs) {
    return !(lhs == rhs);
}

class Participant {
   public:
    UciEngine engine;
    PlayerInfo info;

    // updated time control after each move
    TimeControl time_control;

    [[nodiscard]] int64_t getTimeoutThreshold() const {
        if (info.config.limit.nodes != 0) {
            return 0;
        } else if (info.config.limit.plies != 0) {
            return 0;
        } else if (time_control.fixed_time != 0) {
            return 0;
        } else {
            return time_control.time + 100 /*margin*/;
        }
    }

    [[nodiscard]] bool updateTc(const int64_t elapsed_millis) {
        if (info.config.limit.tc.time == 0) {
            return true;
        }

        time_control.time -= elapsed_millis;

        if (time_control.time < 0) {
            return false;
        }

        time_control.time += time_control.increment;

        return true;
    }
};

}  // namespace fast_chess
