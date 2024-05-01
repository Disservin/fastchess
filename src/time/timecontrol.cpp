#include "timecontrol.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <tuple>

#include <types/enums.hpp>
#include <util/helper.hpp>

#include <json.hpp>

namespace fast_chess {

TimeControl::TimeControl(const Limits &limits) : limits_(limits) {
    if (limits_.fixed_time != 0) {
        time_left_ = limits_.fixed_time;
    } else if (limits_.time != 0) {
        time_left_ = limits_.time;
    } else {
        time_left_ = 0;
    }
}

std::chrono::milliseconds TimeControl::getTimeoutThreshold() const {
    return std::chrono::milliseconds(time_left_ + 100);
}

bool TimeControl::updateTime(const int64_t elapsed_millis) {
    if (limits_.moves > 0) {
        if (moves_left_ == 1) {
            moves_left_ = limits_.moves;
            time_left_ += limits_.time;
        } else {
            moves_left_--;
        }
    }

    if (limits_.fixed_time == 0 && limits_.time == 0) {
        return true;
    }

    time_left_ -= elapsed_millis;

    if (time_left_ < -limits_.timemargin) {
        return false;
    }

    if (time_left_ < 0) {
        time_left_ = 0;
    }

    time_left_ += limits_.increment;

    return true;
}

std::ostream &operator<<(std::ostream &os, const TimeControl &tc) {
    if (tc.limits_.fixed_time > 0) {
        os << std::setprecision(8) << std::noshowpoint << tc.limits_.fixed_time / 1000.0 << "/move";
        return os;
    }
    
    if (tc.moves == 0 && tc.time == 0 && tc.increment == 0 && tc.fixed_time == 0) {
        os << "-";
    }
    
    if (tc.limits_.moves > 0) os << tc.limits_.moves << "/";

    if (tc.limits_.time > 0) os << (tc.limits_.time / 1000.0);

    if (tc.limits_.increment > 0) os << "+" << (tc.limits_.increment / 1000.0);

    return os;
}

}  // namespace fast_chess
