#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <chess.hpp>

#include <core/time/time.hpp>
#include <matchmaking/game_pair.hpp>
#include <types/engine_config.hpp>

namespace fastchess {

struct MoveData {
    MoveData(std::string _move, std::string _score_string, int64_t _elapsed_millis, int _depth, int _seldepth,
             int _score, int _nodes, bool _legal = true, bool _book = false)
        : move(std::move(_move)),
          score_string(std::move(_score_string)),
          elapsed_millis(_elapsed_millis),
          nodes(_nodes),
          seldepth(_seldepth),
          depth(_depth),
          score(_score),
          legal(_legal),
          book(_book) {}

    std::vector<std::string> additional_lines;
    std::string move;
    std::string score_string;
    int64_t elapsed_millis = 0;
    int64_t timeleft       = 0;
    int64_t latency        = 0;
    uint64_t nodes         = 0;
    int seldepth           = 0;
    int depth              = 0;
    int score              = 0;
    uint64_t nps           = 0;
    int hashfull           = 0;
    uint64_t tbhits        = 0;
    bool legal             = true;
    bool book              = false;
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

    explicit MatchData(std::string fen) : fen(std::move(fen)) {
        start_time = time::datetime_iso();
        date       = time::datetime("%Y.%m.%d").value_or("-");
    }

    GamePair<PlayerInfo, PlayerInfo> players;

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
