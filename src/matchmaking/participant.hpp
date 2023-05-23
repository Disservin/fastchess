#pragma once

#include <engines/uci_engine.hpp>
#include <matchmaking/types/player_info.hpp>

namespace fast_chess {

class Participant {
   public:
    explicit Participant(const EngineConfiguration& config) : engine(config) {
        info.config = config;
    }

    [[nodiscard]] int64_t getTimeoutThreshold() const {
        if (engine.getConfig().limit.nodes != 0) {
            return 0;
        } else if (engine.getConfig().limit.plies != 0) {
            return 0;
        } else if (time_control.fixed_time != 0) {
            return 0;
        } else {
            return time_control.time + 100 /*margin*/;
        }
    }

    [[nodiscard]] bool updateTc(const int64_t elapsed_millis) {
        if (engine.getConfig().limit.tc.time == 0) {
            return true;
        }

        time_control.time -= elapsed_millis;

        if (time_control.time < 0) {
            return false;
        }

        time_control.time += time_control.increment;

        return true;
    }

    // updated time control after each move
    TimeControl time_control;

    UciEngine engine;
    PlayerInfo info;
};

}  // namespace fast_chess
