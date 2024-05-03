#pragma once

#include <chrono>

#include <engine/uci_engine.hpp>

namespace fast_chess {

class Player {
   public:
    explicit Player(engine::UciEngine &uci_enigne)
        : engine(uci_enigne), time_control_(uci_enigne.getConfig().limit.tc) {}

    // The timeout threshold for the read engine command.
    // This has nothing to do with the time control itself.
    [[nodiscard]] std::chrono::milliseconds getTimeoutThreshold() const {
        if (engine.getConfig().limit.nodes != 0     //
            || engine.getConfig().limit.plies != 0  //
            || time_control_.isFixedTime()) {
            // no timeout
            return std::chrono::milliseconds(0);
        }

        return time_control_.getTimeoutThreshold();
    }

    // remove the elapsed time from the participant's time control.
    // Returns false if the time control has been exceeded.
    [[nodiscard]] bool updateTime(const int64_t elapsed_millis) {
        return time_control_.updateTime(elapsed_millis);
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
        if (time_control_.isFixedTime()) {
            input << " movetime " << time_control_.getFixedTime();
        } else {
            auto white = stm == chess::Color::WHITE ? time_control_ : enemy_tc;
            auto black = stm == chess::Color::WHITE ? enemy_tc : time_control_;

            if (time_control_.isTimed()) {
                if (white.isTimed()) input << " wtime " << white.getTimeLeft();
                if (black.isTimed()) input << " btime " << black.getTimeLeft();
            }

            if (time_control_.isIncrement()) {
                if (white.isIncrement()) input << " winc " << white.getIncrement();
                if (black.isIncrement()) input << " binc " << black.getIncrement();
            }

            if (time_control_.isMoves()) {
                input << " movestogo " << time_control_.getMovesLeft();
            }
        }
       
        if (engine.getConfig().limit.plies == 0 && engine.getConfig().limit.nodes == 0
           && !time_control_.isTimed() && !time_control_.isFixedTime())
            input << " infinite";

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
