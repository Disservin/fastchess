#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "matchmaking/participant.hpp"

namespace fast_chess
{
struct MoveData
{
    std::string move;
    std::string score_string;
    int64_t elapsed_millis = 0;
    uint64_t nodes = 0;
    int seldepth = 0;
    int depth = 0;
    int score = 0;

    MoveData(std::string _move, std::string _score_string, int64_t _elapsed_millis, int _depth,
             int _seldepth, int _score, int _nodes)
        : move(_move), score_string(std::move(_score_string)), elapsed_millis(_elapsed_millis),
          nodes(_nodes), seldepth(_seldepth), depth(_depth), score(_score)
    {
    }
};

struct MatchData
{
    std::vector<MoveData> moves;
    std::pair<PlayerInfo, PlayerInfo> players;
    std::string termination;
    std::string start_time;
    std::string end_time;
    std::string duration;
    std::string date;
    std::string fen;
    int round = 0;
    bool legal = true;
    bool needs_restart = false;
};
} // namespace fast_chess
