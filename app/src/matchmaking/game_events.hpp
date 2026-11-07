#pragma once

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <chess.hpp>

#include <core/events/events.hpp>
#include <game/timecontrol/timecontrol.hpp>
#include <types/match_data.hpp>
#include <types/score.hpp>

// Helpers to build the JSON objects of the -events stream.
namespace fastchess::events {

// Scores in the event stream are always from White's point of view.
[[nodiscard]] inline json scoreJson(const Score& score, chess::Color mover) {
    const auto value = mover == chess::Color::BLACK ? -score.value : score.value;
    return score.isMate() ? json{{"mate", value}} : json{{"cp", value}};
}

[[nodiscard]] inline std::string_view sideName(chess::Color color) {
    return color == chess::Color::WHITE ? "white" : "black";
}

[[nodiscard]] inline std::string_view terminationName(MatchTermination termination) {
    switch (termination) {
        case MatchTermination::NORMAL:
            return "normal";
        case MatchTermination::ADJUDICATION:
            return "adjudication";
        case MatchTermination::TIMEOUT:
            return "time_forfeit";
        case MatchTermination::ILLEGAL_MOVE:
            return "illegal_move";
        case MatchTermination::DISCONNECT:
            return "disconnect";
        case MatchTermination::STALL:
            return "stall";
        case MatchTermination::INTERRUPT:
        case MatchTermination::None:
            return "interrupted";
    }

    return "interrupted";
}

[[nodiscard]] inline std::vector<std::string> splitPv(std::string_view pv) {
    std::vector<std::string> moves;
    std::size_t start = 0;

    while (start < pv.size()) {
        const auto end = pv.find(' ', start);
        if (end != start) moves.emplace_back(pv.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    return moves;
}

[[nodiscard]] inline std::string tcString(const TimeControl& tc) {
    std::ostringstream ss;
    ss << tc;
    return ss.str();
}

// remaining clock time, null if the player has no clock
[[nodiscard]] inline json clockJson(const TimeControl& tc) {
    if (!tc.isTimed() && !tc.isFixedTime()) return nullptr;
    return std::max<int64_t>(0, tc.getTimeLeft());
}

}  // namespace fastchess::events
