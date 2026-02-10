#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <chess.hpp>

#include <matchmaking/match/match.hpp>

#include <types/tournament.hpp>

#include <game/pgn/openings_data.hpp>
#include <game/pgn/pgn_gen.hpp>

namespace fastchess::pgn {

class PgnBuilder {
   public:
    PgnBuilder(const config::Pgn& pgn_config, const MatchData& match, std::size_t round_id);

    // Get the newly created pgn
    [[nodiscard]] std::string get() const noexcept { return pgn_generator_.generate() + "\n\n"; }

    static constexpr int LINE_LENGTH = 80;

    [[nodiscard]] static std::string convertMatchTermination(const MatchTermination& res) noexcept;

    [[nodiscard]] static std::string getResultFromWhiteMatch(const MatchData::PlayerInfo& white) noexcept;

   private:
    // Converts a UCI move to either SAN, LAN or keeps it as UCI
    [[nodiscard]] std::string moveNotation(chess::Board& board, const std::string& move) const noexcept;

    std::string createComment(const MoveData& move, const MoveData& next_move, bool illegal, bool last) noexcept;

    std::optional<Opening> getOpeningClassification(bool is_frc_variant) const;

    // Adds a comment to the pgn. The comment is formatted as {first, args}
    template <typename First, typename... Args>
    [[nodiscard]] static std::string addComment(First&& first, Args&&... args) {
        std::stringstream ss;

        ss << std::forward<First>(first);
        ((ss << (std::string(args).empty() ? "" : ", ") << std::forward<Args>(args)), ...);

        return ss.str();
    }

    // Formats a time in milliseconds to seconds with 3 decimals
    [[nodiscard]] static std::string formatTime(int64_t millis) {
        std::stringstream ss;
        ss << std::setprecision(3) << std::fixed << millis / 1000.0 << "s";
        return ss.str();
    }

    const config::Pgn& pgn_config_;
    const MatchData& match_;

    PGNGenerator pgn_generator_;
};

}  // namespace fastchess::pgn
