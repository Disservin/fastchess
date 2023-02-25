#pragma once

#include <utility>
#include <vector>

#include "board.h"
#include "engine_config.h"
#include "options.h"
#include "threadpool.h"
#include "uci_engine.h"

struct MoveData
{
    Move move;
    std::string scoreString;
    int depth;
    int64_t elapsedMillis;

    MoveData(Move _move, std::string _scoreString, int _depth, int64_t _elapsedMillis)
        : move(_move), scoreString(std::move(_scoreString)), depth(_depth), elapsedMillis(_elapsedMillis)
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
    Board board;
    int round;
};

struct DrawAdjTracker
{
    Score drawScore;
    int moveCount = 0;

    DrawAdjTracker(Score drawScore, int moveCount)
    {
        this->drawScore = drawScore;
        this->moveCount = moveCount;
    }
};

struct ResignAdjTracker
{
    int moveCount = 0;
    Score resignScore;
    ResignAdjTracker(Score resignScore, int moveCount)
    {
        this->resignScore = resignScore;
        this->moveCount = moveCount;
    }
};

class Tournament
{
  public:
    Tournament() = default;
    Tournament(bool saveTime) : saveTimeHeader(saveTime){};
    Tournament(const CMD::GameManagerOptions &mc);

    void loadConfig(const CMD::GameManagerOptions &mc);
    void startTournament(std::vector<EngineConfiguration> configs);

    Match startMatch(UciEngine &engine1, UciEngine &engine2, int round, std::string openingFen);
    std::vector<Match> runH2H(CMD::GameManagerOptions localMatchConfig, std::vector<EngineConfiguration> configs,
                              int gameId, std::string fen);

    std::string fetchNextFen();

    std::vector<std::string> getPGNS() const;

    template <typename T> static T findElement(const std::vector<std::string> &haystack, std::string_view needle)
    {
        int index = std::find(haystack.begin(), haystack.end(), needle) - haystack.begin();
        if constexpr (std::is_same_v<T, int>)
            return std::stoi(haystack[index + 1]);
        else
            return haystack[index + 1];
    }

  private:
    CMD::GameManagerOptions matchConfig;

    ThreadPool pool = ThreadPool(1);

    std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");
    std::string formatDuration(std::chrono::seconds duration);
    void updateTrackers(DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker, const Score moveScore);
    GameResult CheckAdj(const int moveNumber, const DrawAdjTracker drawTracker, const ResignAdjTracker resignTracker,
                        const Score score, const Color lastSideThatMoved);

    std::vector<std::string> pgns;
    std::vector<std::string> openingBook;

    size_t startIndex = 0;

    bool saveTimeHeader = true;
};