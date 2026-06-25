#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <chess.hpp>

#include <core/time/time.hpp>
#include <matchmaking/game_pair.hpp>
#include <types/engine_config.hpp>
#include <types/score.hpp>

namespace fastchess {

struct MoveData {
    MoveData() = default;
    explicit MoveData(std::string _move) : move(std::move(_move)) {}
    MoveData(std::string _move, Score _score, int64_t _elapsed_millis, int64_t _depth, int64_t _seldepth,
             uint64_t _nodes, bool _legal = true)
        : move(std::move(_move)),
          ponder(""),
          nodes(_nodes),
          elapsed_millis(_elapsed_millis),
          depth(_depth),
          seldepth(_seldepth),
          score(_score),
          legal(_legal) {}

    std::vector<std::string> additional_lines;
    std::string move;
    std::string ponder;
    std::string pv = "";

    uint64_t nodes  = 0;
    uint64_t nps    = 0;
    uint64_t tbhits = 0;

    int64_t elapsed_millis     = 0;
    int64_t depth              = 0;
    int64_t seldepth           = 0;
    std::optional<Score> score = std::nullopt;
    int64_t timeleft           = 0;
    int64_t latency            = 0;
    int64_t hashfull           = 0;

    bool legal = true;
    bool book  = false;
};

enum class MatchTermination {
    NORMAL,
    ADJUDICATION,
    TIMEOUT,
    DISCONNECT,
    STALL,
    ILLEGAL_MOVE,
    INTERRUPT,
    None,
};

struct MatchData {
    struct PlayerInfo {
        EngineConfiguration config;
        chess::GameResult result = chess::GameResult::NONE;
    };

    MatchData() {}

    explicit MatchData(std::string_view f) : fen(f) {
        start_time = time::datetime_iso();
        date       = time::datetime("%Y.%m.%d").value_or("-");
    }

    GamePair<PlayerInfo, PlayerInfo> players;

    std::optional<PlayerInfo> getLosingPlayer() const {
        const auto check = [&](const PlayerInfo& player) { return player.result == chess::GameResult::LOSE; };

        if (check(players.white)) return players.white;
        if (check(players.black)) return players.black;
        return std::nullopt;
    }

    std::optional<PlayerInfo> getWinningPlayer() const {
        const auto check = [&](const PlayerInfo& player) { return player.result == chess::GameResult::WIN; };

        if (check(players.white)) return players.white;
        if (check(players.black)) return players.black;
        return std::nullopt;
    }

    std::vector<std::string_view> getMoves() const {
        std::vector<std::string_view> result;
        result.reserve(moves.size());

        for (const auto& data : moves) {
            result.emplace_back(data.move);
        }

        return result;
    }

    std::string start_time;
    std::string end_time;
    std::string duration;
    std::string date;
    std::string fen;

    // This reason will be printed on the console and will be written to the
    // pgn file as a comment at the end of the game.
    std::string reason;

    std::vector<MoveData> moves;

    // This is the reason why the game ended.
    // It will be used for the PGN Header.
    MatchTermination termination = MatchTermination::None;

    VariantType variant = VariantType::STANDARD;

    bool needs_restart = false;
};

}  // namespace fastchess
