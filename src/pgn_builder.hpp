#pragma once

#include "matchmaking/match.hpp"
#include "options.hpp"
#include "third_party/chess.hpp"

namespace fast_chess {

class PgnBuilder {
   public:
    PgnBuilder(const MatchData &match, const CMD::GameManagerOptions &game_options);

    std::string get() const { return pgn_.str(); }

   private:
    void addHeader(const std::string &name, const std::string &value);

    void addMove(Chess::Board &board, const MoveData &move, std::size_t move_number);

    template <typename First, typename... Args>
    static std::string addComment(First &&first, Args &&...args) {
        std::stringstream ss;
        ss << " { " << std::forward<First>(first);
        ((ss << ", " << std::forward<Args>(args)), ...);
        ss << " }";
        return ss.str();
    }

    static std::string formatTime(uint64_t millis) {
        std::stringstream ss;
        ss << std::setprecision(3) << std::fixed << millis / 1000.0;
        return ss.str();
    }

    std::string getResultFromMatch(const MatchData &match) const;

    std::stringstream pgn_;
    MatchData match_;
    CMD::GameManagerOptions game_options_;
};

}  // namespace fast_chess