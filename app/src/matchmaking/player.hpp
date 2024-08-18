#pragma once

#include <chrono>

#include <engine/uci_engine.hpp>

namespace fastchess {

class Player {
   public:
    explicit Player(engine::UciEngine &uci_enigne)
        : engine(uci_enigne), time_control_(uci_enigne.getConfig().limit.tc) {}

    // The timeout threshold for the read engine command.
    // This has nothing to do with the time control itself.
    [[nodiscard]] std::chrono::milliseconds getTimeoutThreshold() const {
        if (engine.getConfig().limit.nodes != 0 || engine.getConfig().limit.plies != 0) {
            // no timeout
            return std::chrono::milliseconds(0);
        }

        return time_control_.getTimeoutThreshold();
    }

    // remove the elapsed time from the participant's time control.
    // Returns false if the time control has been exceeded.
    [[nodiscard]] bool updateTime(const int64_t elapsed_millis) noexcept {
        return time_control_.updateTime(elapsed_millis);
    }

    [[nodiscard]] const TimeControl &getTimeControl() const noexcept { return time_control_; }

    void setLost() noexcept { result = chess::GameResult::LOSE; }
    void setDraw() noexcept { result = chess::GameResult::DRAW; }
    void setWon() noexcept { result = chess::GameResult::WIN; }

    [[nodiscard]] chess::GameResult getResult() const noexcept { return result; }

    engine::UciEngine &engine;

    chess::Color color = chess::Color::NONE;

   private:
    chess::GameResult result = chess::GameResult::NONE;
    TimeControl time_control_;
};

}  // namespace fastchess
