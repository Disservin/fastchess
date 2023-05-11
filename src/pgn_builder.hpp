#pragma once

#include <third_party/chess.hpp>

#include <matchmaking/match.hpp>
#include <types.hpp>

namespace fast_chess {

class PgnBuilder {
   public:
    PgnBuilder(const MatchData &match, const cmd::GameManagerOptions &game_options);

    std::string get() const { return pgn_.str() + "\n\n"; }

    static constexpr int LINE_LENGTH = 80;

   private:
    std::string moveNotation(Chess::Board &board, const std::string &move) const;

    template <typename T>
    void addHeader(const std::string &name, const T &value);

    void addMove(Chess::Board &board, const MoveData &move, std::size_t move_number);

    template <typename First, typename... Args>
    static std::string addComment(First &&first, Args &&...args) {
        std::stringstream ss;
        ss << " {" << std::forward<First>(first);
        ((ss << ", " << std::forward<Args>(args)), ...);
        ss << "}";
        return ss.str();
    }

    static std::string formatTime(uint64_t millis) {
        std::stringstream ss;
        ss << std::setprecision(3) << std::fixed << millis / 1000.0 << "s";
        return ss.str();
    }

    std::string getResultFromMatch(const MatchData &match) const;

    MatchData match_;
    cmd::GameManagerOptions game_options_;

    std::stringstream pgn_;
    std::vector<std::string> moves_;
};

}  // namespace fast_chess