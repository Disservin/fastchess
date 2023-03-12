#pragma once

#include <cstdint>
#include <string>

#include "chess/types.hpp"
#include "engines/engine_config.hpp"

struct MoveData
{
    std::string move;
    std::string scoreString;
    int64_t elapsedMillis = 0;
    int depth = 0;
    int score = 0;
    uint64_t nodes = 0;
    MoveData() = default;

    MoveData(std::string _move, std::string _scoreString, int64_t _elapsedMillis, int _depth,
             int _score, int _nodes)
        : move(_move), scoreString(std::move(_scoreString)), elapsedMillis(_elapsedMillis),
          depth(_depth), score(_score), nodes(_nodes)
    {
    }
};

struct Match
{
    std::vector<MoveData> moves;
    EngineConfiguration whiteEngine;
    EngineConfiguration blackEngine;
    GameResult result;
    std::string termination;
    std::string startTime;
    std::string endTime;
    std::string duration;
    std::string date;
    std::string fen;
    int round = 0;
    bool legal = true;
    bool needsRestart = false;
};

struct DrawAdjTracker
{
    Score drawScore = 0;
    int moveCount = 0;

    DrawAdjTracker() = default;
    DrawAdjTracker(Score drawScore, int moveCount)
    {
        this->drawScore = drawScore;
        this->moveCount = moveCount;
    }
};

struct ResignAdjTracker
{
    int moveCount = 0;
    Score resignScore = 0;

    ResignAdjTracker() = default;
    ResignAdjTracker(Score resignScore, int moveCount)
    {
        this->resignScore = resignScore;
        this->moveCount = moveCount;
    }
};

struct Stats
{
    int wins = 0;
    int draws = 0;
    int losses = 0;

    int pentaWW = 0;
    int pentaWD = 0;
    int pentaWL = 0;
    int pentaLD = 0;
    int pentaLL = 0;

    int roundCount = 0;
    int totalCount = 0;
    int timeouts = 0;

    Stats() = default;

    Stats(int _wins, int _draws, int _losses, int _pentaWW, int _pentaWD, int _pentaWL,
          int _pentaLD, int _pentaLL, int _roundCount, int _totalCount, int _timeouts)
        : wins(_wins), draws(_draws), losses(_losses), pentaWW(_pentaWW), pentaWD(_pentaWD),
          pentaWL(_pentaWL), pentaLD(_pentaLD), pentaLL(_pentaLL), roundCount(_roundCount),
          totalCount(_totalCount), timeouts(_timeouts){};
};
