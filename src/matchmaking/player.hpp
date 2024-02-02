#pragma once

#include <chrono>

#include <engines/uci_engine.hpp>

namespace fast_chess {

class Player {
    struct PreciseTime {
        std::chrono::nanoseconds time;
        std::chrono::nanoseconds increment;
        std::chrono::nanoseconds fixed_time;
        int moves;

        PreciseTime() = default;

        PreciseTime(uint64_t time, uint64_t increment, uint64_t fixed_time, int moves)
            : time(std::chrono::milliseconds(time)),
              increment(std::chrono::milliseconds(increment)),
              fixed_time(std::chrono::milliseconds(fixed_time)),
              moves(moves) {}
    };

   public:
    explicit Player(UciEngine &uci_enigne) : engine(uci_enigne) {
        // copy time control which will be updated later
        // time_control_ = engine.getConfig().limit.tc;
        const auto config = engine.getConfig();
        time_control_     = PreciseTime(config.limit.tc.time, config.limit.tc.increment,
                                        config.limit.tc.fixed_time, config.limit.tc.moves);
    }

    /// @brief The timeout threshold for the read engine command.
    /// This has nothing to do with the time control itself.
    /// @return time in ms
    [[nodiscard]] std::chrono::milliseconds getTimeoutThreshold() const {
        if (engine.getConfig().limit.nodes != 0     //
            || engine.getConfig().limit.plies != 0  //
            || time_control_.fixed_time.count() != 0) {
            // no timeout
            return std::chrono::milliseconds(0);
        }

        return std::chrono::milliseconds(time_control_.time.count() + 100) /* margin*/;
    }

    /// @brief remove the elapsed time from the participant's time
    /// @param elapsed_millis
    /// @return `false` when out of time
    [[nodiscard]] bool updateTime(std::chrono::nanoseconds used_time) {
        if (engine.getConfig().limit.tc.time == 0) {
            return true;
        }

        time_control_.time -= used_time;

        if (time_control_.time.count() < 0) {
            return false;
        }

        time_control_.time += time_control_.increment;

        return true;
    }

    /// @brief Build the uci position input from the given moves and fen.
    /// @param moves
    /// @param fen
    /// @return
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

    /// @brief Build the uci go input from the given time controls.
    /// @param stm
    /// @param enemy_tc
    /// @return
    [[nodiscard]] std::string buildGoInput(chess::Color stm, const PreciseTime &enemy_tc) const {
        std::stringstream input;
        input << "go";

        if (engine.getConfig().limit.nodes != 0)
            input << " nodes " << engine.getConfig().limit.nodes;

        if (engine.getConfig().limit.plies != 0)
            input << " depth " << engine.getConfig().limit.plies;

        // We cannot use st and tc together
        if (time_control_.fixed_time.count() != 0) {
            const auto mt =
                std::chrono::duration_cast<std::chrono::milliseconds>(time_control_.fixed_time)
                    .count();
            input << " movetime " << mt;
        } else if (time_control_.time.count() != 0 && enemy_tc.time.count() != 0) {
            auto white = stm == chess::Color::WHITE ? time_control_ : enemy_tc;
            auto black = stm == chess::Color::WHITE ? enemy_tc : time_control_;

            input << " wtime "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(white.time).count()
                  << " btime "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(black.time).count();

            if (time_control_.increment.count() != 0) {
                input << " winc "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(white.increment)
                             .count()
                      << " binc "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(black.increment)
                             .count();
            }

            if (time_control_.moves != 0) {
                input << " movestogo " << time_control_.moves;
            }
        }

        return input.str();
    }

    [[nodiscard]] const PreciseTime &getTimeControl() const { return time_control_; }

    UciEngine &engine;

    chess::Color color       = chess::Color::NONE;
    chess::GameResult result = chess::GameResult::NONE;

   private:
    /// @brief updated time control after each move
    PreciseTime time_control_;
};

}  // namespace fast_chess
