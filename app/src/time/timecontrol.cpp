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
    } else {
        time_left_ = limits_.time + limits_.increment;
    }

    moves_left_ = limits_.moves;
}

std::chrono::milliseconds TimeControl::getTimeoutThreshold() const noexcept {
    return std::chrono::milliseconds(time_left_ + limits_.timemargin + MARGIN);
}

bool TimeControl::updateTime(const int64_t elapsed_millis) noexcept {
    if (limits_.moves > 0) {
        if (moves_left_ == 1) {
            moves_left_ = limits_.moves;
            time_left_ += limits_.time;
        } else {
            moves_left_--;
        }
    }

    if (limits_.fixed_time == 0 && limits_.time + limits_.increment == 0) {
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

    if (limits_.fixed_time != 0) time_left_ = limits_.fixed_time;

    return true;
}

std::ostream &operator<<(std::ostream &os, const TimeControl &tc) {
    if (tc.limits_.fixed_time > 0) {
        os << std::setprecision(8) << std::noshowpoint << tc.limits_.fixed_time / 1000.0 << "/move";
        return os;
    }

    if (tc.limits_.moves == 0 && tc.limits_.time == 0 && tc.limits_.increment == 0) {
        os << "-";
    }

    if (tc.limits_.moves > 0) os << tc.limits_.moves << "/";

    if (tc.limits_.time > 0) os << (tc.limits_.time / 1000.0);

    if (tc.limits_.increment > 0) os << "+" << (tc.limits_.increment / 1000.0);

    return os;
}

}  // namespace fast_chess
