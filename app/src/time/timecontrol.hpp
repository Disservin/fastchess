#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <tuple>

#include <types/enums.hpp>
#include <util/helper.hpp>

#include <json.hpp>

namespace mercury {
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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_ORDERED_JSON(Limits, increment, fixed_time, time, moves, timemargin)
    };

    TimeControl() = default;

    TimeControl(const Limits &limits);

    [[nodiscard]] std::chrono::milliseconds getTimeoutThreshold() const noexcept;
    [[nodiscard]] bool updateTime(const int64_t elapsed_millis) noexcept;

    [[nodiscard]] int64_t getTimeLeft() const { return time_left_; }
    [[nodiscard]] int64_t getMovesLeft() const { return moves_left_; }
    [[nodiscard]] int64_t getFixedTime() const { return limits_.fixed_time; }
    [[nodiscard]] int64_t getIncrement() const { return limits_.increment; }

    void setMoves(int64_t moves) { limits_.moves = moves; }
    void setIncrement(int64_t inc) { limits_.increment = inc; }
    void setTime(int64_t time) { limits_.time = time; }
    void setFixedTime(int64_t fixed_time) { limits_.fixed_time = fixed_time; }
    void setTimemargin(int64_t timemargin) { limits_.timemargin = timemargin; }

    [[nodiscard]] bool isFixedTime() const noexcept { return limits_.fixed_time != 0; }
    [[nodiscard]] bool isTimed() const noexcept { return limits_.time != 0; }
    [[nodiscard]] bool isMoves() const noexcept { return limits_.moves != 0; }
    [[nodiscard]] bool isIncrement() const noexcept { return limits_.increment != 0; }

    friend std::ostream &operator<<(std::ostream &os, const TimeControl &tc);

    bool operator==(const TimeControl &rhs) const {
        return limits_ == rhs.limits_ && time_left_ == rhs.time_left_ && moves_left_ == rhs.moves_left_;
    }

    static constexpr int64_t MARGIN = 100;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ORDERED_JSON(TimeControl, limits_, time_left_, moves_left_)
   private:
    Limits limits_;

    int64_t time_left_;
    int moves_left_;
};

}  // namespace mercury
