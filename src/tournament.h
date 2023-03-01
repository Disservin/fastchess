#pragma once

#include <atomic>
#include <fstream>
#include <utility>
#include <vector>

#include "board.h"
#include "engine_config.h"
#include "options.h"
#include "sprt.h"
#include "threadpool.h"
#include "uci_engine.h"

struct MoveData
{
    std::string move;
    std::string scoreString;
    int64_t elapsedMillis = 0;
    int depth = 0;
    int score = 0;
    uint64_t nodes = 0;
    MoveData() = default;

    MoveData(std::string _move, std::string _scoreString, int64_t _elapsedMillis, int _depth, int _score, int _nodes)
        : move(_move), scoreString(std::move(_scoreString)), elapsedMillis(_elapsedMillis), depth(_depth),
          score(_score), nodes(_nodes)
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

/*
 * This is the main class to start engines matches.
 * Generally we always swap the engine order, the first engine is always
 * the the positive engine, meaning all stats should be calculated from that view point.
 */
class Tournament
{
  public:
    template <typename T> static T findElement(const std::vector<std::string> &haystack, std::string_view needle)
    {
        auto index = std::find(haystack.begin(), haystack.end(), needle) - haystack.begin();
        if constexpr (std::is_same_v<T, int>)
            return std::stoi(haystack[index + 1]);
        else if constexpr (std::is_same_v<T, float>)
            return std::stof(haystack[index + 1]);
        else if constexpr (std::is_same_v<T, uint64_t>)
            return std::stoull(haystack[index + 1]);
        else
            return haystack[index + 1];
    }

    Tournament() = default;

    Tournament(bool saveTime) : saveTimeHeader(saveTime){};

    Tournament(const CMD::GameManagerOptions &mc);

    void loadConfig(const CMD::GameManagerOptions &mc);

    std::vector<std::string> getPGNS() const;

    void setStorePGN(bool v);
    
    void printElo();

    void startTournament(const std::vector<EngineConfiguration> &configs);

    void stopPool();

  private:
    const std::string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const Score MATE_SCORE = 100'000;

    CMD::GameManagerOptions matchConfig = {};

    ThreadPool pool = ThreadPool(1);

    SPRT sprt;

    void writeToFile(const std::string &data);
    
    std::string fetchNextFen();

    void playNextMove(UciEngine &engine, std::string &positionInput, Board &board, TimeControl &timeLeftUs,
                      const TimeControl &timeLeftThem, GameResult &res, Match &match, DrawAdjTracker &drawTracker,
                      ResignAdjTracker &resignTracker, int &retflag, int roundId);

    Match startMatch(UciEngine &engine1, UciEngine &engine2, int roundId, std::string openingFen);

    std::vector<Match> runH2H(CMD::GameManagerOptions localMatchConfig, const std::vector<EngineConfiguration> &configs,
                              int roundId, const std::string &fen);

    MoveData parseEngineOutput(const std::vector<std::string> &output, const std::string &move, int64_t measuredTime);

    std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");
    
    std::string formatDuration(std::chrono::seconds duration);

    void updateTrackers(DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker, const Score moveScore);

    GameResult checkAdj(Match &match, const DrawAdjTracker drawTracker, const ResignAdjTracker resignTracker,
                        const Score score, const Color lastSideThatMoved) const;

    void checkEngineStatus(UciEngine &engine, Match &match, int &retflag, int roundId) const;

    std::vector<std::string> pgns;
    std::vector<std::string> openingBook;
    std::vector<std::string> engineNames;

    std::mutex fileMutex;

    std::atomic<int> wins = 0;
    std::atomic<int> draws = 0;
    std::atomic<int> losses = 0;
    std::atomic<int> roundCount = 0;
    std::atomic<int> totalCount = 0;

    std::atomic<int> pentaWW = 0;
    std::atomic<int> pentaWD = 0;
    std::atomic<int> pentaWL = 0;
    std::atomic<int> pentaLD = 0;
    std::atomic<int> pentaLL = 0;

    std::ofstream file;

    size_t startIndex = 0;

    bool storePGNS = false;
    bool saveTimeHeader = true;
};
