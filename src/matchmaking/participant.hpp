#pragma once

#include <engines/uci_engine.hpp>
#include <types/player_info.hpp>

namespace fast_chess {

class Participant {
   public:
    explicit Participant(const EngineConfiguration &config, std::size_t core)
        : engine(config, core) {
        info.config = config;

        // copy time control which will be updated later
        time_control_ = engine.getConfig().limit.tc;
    }

    /// @brief The timeout threshold for the read engine command.
    /// This has nothing to do with the time control itself.
    /// @return time in ms
    [[nodiscard]] int64_t getTimeoutThreshold() const {
        if (engine.getConfig().limit.nodes != 0 || engine.getConfig().limit.plies != 0 ||
            time_control_.fixed_time != 0) {
            return 0;  // no timeout
        } else {
            return time_control_.time + 100 /* margin*/;
        }
    }

    /// @brief remove the elapsed time from the participant's time
    /// @param elapsed_millis
    /// @return `false` when out of time
    [[nodiscard]] bool updateTime(const int64_t elapsed_millis) {
        if (engine.getConfig().limit.tc.time == 0) {
            return true;
        }

        time_control_.time -= elapsed_millis;

        if (time_control_.time < 0) {
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
        } else if (time_control_.time != 0 && enemy_tc.time != 0) {
            auto white = stm == chess::Color::WHITE ? time_control_ : enemy_tc;
            auto black = stm == chess::Color::WHITE ? enemy_tc : time_control_;

            input << " wtime " << white.time << " btime " << black.time;

            if (time_control_.increment != 0) {
                input << " winc " << white.increment << " binc " << black.increment;
            }

            if (time_control_.moves != 0) {
                input << " movestogo " << time_control_.moves;
            }
        }

        return input.str();
    }

    [[nodiscard]] const TimeControl &getTimeControl() const { return time_control_; }

    UciEngine engine;
    PlayerInfo info;

   private:
    /// @brief updated time control after each move
    TimeControl time_control_;
};

}  // namespace fast_chess
