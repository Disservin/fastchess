#pragma once

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <types/enums.hpp>
#include <util/helper.hpp>

#include <json.hpp>

namespace fast_chess {
// @todo use std::chrono::milliseconds
class TimeControl {
   public:
    struct Limits {
        // go winc/binc, in milliseconds
        int64_t increment = 0;
        // go movetime, in milliseconds
        int64_t fixed_time = 0;
        // go wtime/btime, in milliseconds
        int64_t time = 0;
        // go movestogo
        int64_t moves = 0;

        int64_t timemargin = 0;

        bool operator==(const Limits &rhs) const {
            return std::tie(increment, fixed_time, time, moves, timemargin) ==
                   std::tie(rhs.increment, rhs.fixed_time, rhs.time, rhs.moves, rhs.timemargin);
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_ORDERED_JSON(Limits, increment, fixed_time, time, moves,
                                                    timemargin)
    };

    TimeControl() = default;

    TimeControl(const Limits &limits) : limits_(limits) {
        if (limits_.fixed_time != 0) {
            time_left_ = limits_.fixed_time;
        } else if (limits_.time != 0) {
            time_left_ = limits_.time;
        } else {
            time_left_ = std::numeric_limits<std::int64_t>::max();
        }
    }

    [[nodiscard]] std::chrono::milliseconds getTimeoutThreshold() const {
        return std::chrono::milliseconds(time_left_ + 100);
    }

    [[nodiscard]] bool updateTime(const int64_t elapsed_millis) {
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

    [[nodiscard]] int64_t getTimeLeft() const { return time_left_; }
    [[nodiscard]] int64_t getMovesLeft() const { return moves_left_; }
    [[nodiscard]] int64_t getFixedTime() const { return limits_.fixed_time; }
    [[nodiscard]] int64_t getIncrement() const { return limits_.increment; }

    void setMoves(int64_t moves) { limits_.moves = moves; }
    void setIncrement(int64_t inc) { limits_.increment = inc; }
    void setTime(int64_t time) { limits_.time = time; }
    void setFixedTime(int64_t fixed_time) { limits_.fixed_time = fixed_time; }
    void setTimemargin(int64_t timemargin) {
        limits_.timemargin = timemargin;

        if (timemargin < 0) {
            throw std::runtime_error("Error; timemargin cannot be a negative number");
        }
    }

    [[nodiscard]] bool isFixedTime() const noexcept { return limits_.fixed_time != 0; }
    [[nodiscard]] bool isTimed() const noexcept { return limits_.time != 0; }
    [[nodiscard]] bool isMoves() const noexcept { return limits_.moves != 0; }
    [[nodiscard]] bool isIncrement() const noexcept { return limits_.increment != 0; }

    friend std::ostream &operator<<(std::ostream &os, const TimeControl &tc);

    bool operator==(const TimeControl &rhs) const {
        return limits_ == rhs.limits_ && time_left_ == rhs.time_left_ &&
               moves_left_ == rhs.moves_left_;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ORDERED_JSON(TimeControl, limits_, time_left_, moves_left_)
   private:
    Limits limits_;

    int64_t time_left_;
    int moves_left_;
};

inline std::ostream &operator<<(std::ostream &os, const TimeControl &tc) {
    if (tc.limits_.fixed_time > 0) {
        os << std::setprecision(8) << std::noshowpoint << tc.limits_.fixed_time / 1000.0 << "/move";
        return os;
    }

    if (tc.limits_.moves > 0) os << tc.limits_.moves << "/";

    os << (tc.limits_.time / 1000.0);

    if (tc.limits_.increment > 0) os << "+" << (tc.limits_.increment / 1000.0);

    return os;
}

}  // namespace fast_chess
