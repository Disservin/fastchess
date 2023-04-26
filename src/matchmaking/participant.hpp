#pragma once

#include "../engines/uci_engine.hpp"
#include "./types/player_info.hpp"

namespace fast_chess {

class Participant {
   public:
    [[nodiscard]] int64_t getTimeoutThreshold() const {
        if (info_.config.limit.nodes != 0) {
            return 0;
        } else if (info_.config.limit.plies != 0) {
            return 0;
        } else if (time_control_.fixed_time != 0) {
            return 0;
        } else {
            return time_control_.time + 100 /*margin*/;
        }
    }

    [[nodiscard]] bool updateTc(const int64_t elapsed_millis) {
        if (info_.config.limit.tc.time == 0) {
            return true;
        }

        time_control_.time -= elapsed_millis;

        if (time_control_.time < 0) {
            return false;
        }

        time_control_.time += time_control_.increment;

        return true;
    }

    // updated time control after each move
    TimeControl time_control_;

    UciEngine engine_;
    PlayerInfo info_;
};

}  // namespace fast_chess
