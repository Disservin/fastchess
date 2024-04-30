#pragma once

#include <chrono>

#include <engine/uci_engine.hpp>

namespace fast_chess {

class Player {
   public:
    explicit Player(engine::UciEngine &uci_enigne)
        : engine(uci_enigne), time_control_(uci_enigne.getConfig().limit.tc) {
        if (time_control_.fixed_time != 0) {
            time_control_.time_left = time_control_.fixed_time;
        } else if (time_control_.time != 0) {
            time_control_.time_left = time_control_.time;
        } else {
            time_control_.time_left = std::numeric_limits<std::int64_t>::max();
        }

        time_control_.moves_left = time_control_.moves;
    }

    // The timeout threshold for the read engine command.
    // This has nothing to do with the time control itself.
    [[nodiscard]] std::chrono::milliseconds getTimeoutThreshold() const {
        if (engine.getConfig().limit.nodes != 0     //
            || engine.getConfig().limit.plies != 0  //
            || time_control_.fixed_time != 0) {
            // no timeout
            return std::chrono::milliseconds(0);
        }

        return std::chrono::milliseconds(time_control_.time_left + 100) /* margin*/;
    }

    // remove the elapsed time from the participant's time control.
    // Returns false if the time control has been exceeded.
    [[nodiscard]] bool updateTime(const int64_t elapsed_millis) {
        // no time control, i.e. fixed nodes
        if (time_control_.moves > 0) {
            // new tc
            if (time_control_.moves_left == 1) {
                time_control_.moves_left = time_control_.moves;
                time_control_.time_left += time_control_.time;
            } else {
                time_control_.moves_left--;
            }
        }

        if (time_control_.fixed_time == 0 && time_control_.time == 0) {
            return true;
        }

        auto &tc = time_control_;
        tc.time_left -= elapsed_millis;

        if (tc.time_left < -tc.timemargin) {
            return false;
        }

        if (tc.time_left < 0) {
            tc.time_left = 0;
        }

        tc.time_left += time_control_.increment;

        return true;
    }

    // Build the uci position input from the given moves and fen.
    [[nodiscard]] static std::string buildPositionInput(const std::vector<std::string> &moves,
                                                        const std::string &fen) {
        std::string position = fen == "startpos" ? "position startpos" : ("position fen " + fen);

        if (!moves.empty()) {
            position += " moves";
            for (const auto &move : moves) {
                position += " " + move;
            }
        }

        return position;
    }

    // Build the uci go input from the given time controls.
    [[nodiscard]] std::string buildGoInput(chess::Color stm, const TimeControl &enemy_tc) const {
        std::stringstream input;
        input << "go";

        if (engine.getConfig().limit.nodes != 0)
            input << " nodes " << engine.getConfig().limit.nodes;

        if (engine.getConfig().limit.plies != 0)
            input << " depth " << engine.getConfig().limit.plies;

        // We cannot use st and tc together
        if (time_control_.fixed_time != 0) {
            input << " movetime " << time_control_.fixed_time;
        } else {
            auto white = stm == chess::Color::WHITE ? time_control_ : enemy_tc;
            auto black = stm == chess::Color::WHITE ? enemy_tc : time_control_;

            if (time_control_.time != 0) {
                input << " wtime " << white.time_left << " btime " << black.time_left;
            }

            if (time_control_.increment != 0) {
                input << " winc " << white.increment << " binc " << black.increment;
            }

            if (time_control_.moves != 0) {
                input << " movestogo " << time_control_.moves_left;
            }
        }

        return input.str();
    }

    [[nodiscard]] const TimeControl &getTimeControl() const { return time_control_; }

    engine::UciEngine &engine;

    chess::Color color       = chess::Color::NONE;
    chess::GameResult result = chess::GameResult::NONE;

   private:
    TimeControl time_control_;
};

}  // namespace fast_chess
