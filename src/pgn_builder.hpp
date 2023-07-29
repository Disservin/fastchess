#pragma once

#include <third_party/chess.hpp>

#include <matchmaking/match.hpp>
#include <types.hpp>

namespace fast_chess {

class PgnBuilder {
   public:
    PgnBuilder(const MatchData &match, const cmd::GameManagerOptions &game_options);

    /// @brief Get the newly created pgn
    /// @return
    std::string get() const { return pgn_.str() + "\n\n"; }

    static constexpr int LINE_LENGTH = 80;

   private:
    /// @brief Converts a UCI move to either SAN, LAN or keeps it as UCI
    /// @param board
    /// @param move
    /// @return
    std::string moveNotation(chess::Board &board, const std::string &move) const;

    /// @brief Adds a header to the pgn
    /// @tparam T
    /// @param name
    /// @param value
    template <typename T>
    void addHeader(const std::string &name, const T &value);

    void addMove(chess::Board &board, const MoveData &move, std::size_t move_number);

    /// @brief Adds a comment to the pgn. The comment is formatted as {first, args}
    /// @tparam First
    /// @tparam ...Args
    /// @param first
    /// @param ...args
    /// @return
    template <typename First, typename... Args>
    static std::string addComment(First &&first, Args &&...args) {
        std::stringstream ss;

        ss << " {" << std::forward<First>(first);
        ((ss << (std::string(args).empty() ? "" : ", ") << std::forward<Args>(args)), ...);
        ss << "}";

        return ss.str();
    }

    /// @brief Formats a time in milliseconds to seconds with 3 decimals
    /// @param millis
    /// @return
    static std::string formatTime(uint64_t millis) {
        std::stringstream ss;
        ss << std::setprecision(3) << std::fixed << millis / 1000.0 << "s";
        return ss.str();
    }

    static std::string getResultFromMatch(const MatchData &match);

    MatchData match_;
    cmd::GameManagerOptions game_options_;

    std::stringstream pgn_;
    std::vector<std::string> moves_;
};

}  // namespace fast_chess