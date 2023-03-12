#pragma once

#include <atomic>
#include <fstream>
#include <utility>
#include <vector>

#include "chess/board.hpp"
#include "engines/engine_config.hpp"
#include "engines/uci_engine.hpp"
#include "options.hpp"
#include "sprt.hpp"
#include "threadpool.hpp"

namespace fast_chess
{

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
    int move_count = 0;

    DrawAdjTracker() = default;
    DrawAdjTracker(Score drawScore, int move_count)
    {
        this->drawScore = drawScore;
        this->move_count = move_count;
    }
};

struct ResignAdjTracker
{
    int move_count = 0;
    Score resignScore = 0;

    ResignAdjTracker() = default;
    ResignAdjTracker(Score resignScore, int move_count)
    {
        this->resignScore = resignScore;
        this->move_count = move_count;
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
    template <typename T>
    static T findElement(const std::vector<std::string> &haystack, std::string_view needle)
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

    // For testing purposes
    Tournament(bool saveTime) : save_time_header_(saveTime)
    {
        file_.open("fast-chess.pgn", std::ios::app);
    };

    Tournament(const CMD::GameManagerOptions &mc);

    void loadConfig(const CMD::GameManagerOptions &mc);

    std::vector<std::string> getPGNS() const;

    void setStorePGN(bool v);

    void printElo();

    void startTournament(const std::vector<EngineConfiguration> &configs);

    void stopPool();

  private:
    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const Score mate_score_ = 100'000;
    CMD::GameManagerOptions match_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = {};

    std::vector<std::string> pgns_;
    std::vector<std::string> opening_book_;
    std::vector<std::string> engine_names_;

    std::ofstream file_;
    std::mutex file_mutex_;

    std::atomic<int> wins_ = 0;
    std::atomic<int> draws_ = 0;
    std::atomic<int> losses_ = 0;
    std::atomic<int> round_count_ = 0;
    std::atomic<int> total_count_ = 0;
    std::atomic<int> timeouts_ = 0;

    std::atomic<int> penta_WW_ = 0;
    std::atomic<int> penta_WD_ = 0;
    std::atomic<int> penta_WL_ = 0;
    std::atomic<int> penta_LD_ = 0;
    std::atomic<int> penta_LL_ = 0;

    size_t fen_index_ = 0;

    bool store_pgns_ = false;

    bool save_time_header_ = true;

    void writeToFile(const std::string &data);

    std::string fetchNextFen();

    bool playNextMove(UciEngine &engine, std::string &positionInput, Board &board,
                      TimeControl &timeLeftUs, const TimeControl &timeLeftThem, GameResult &res,
                      Match &match, DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker,
                      int roundId);

    Match startMatch(UciEngine &engine1, UciEngine &engine2, int roundId, std::string openingFen);

    std::vector<Match> runH2H(CMD::GameManagerOptions localMatchConfig,
                              const std::vector<EngineConfiguration> &configs, int roundId,
                              const std::string &fen);

    MoveData parseEngineOutput(const Board &board, const std::vector<std::string> &output,
                               const std::string &move, int64_t measuredTime);

    void updateTrackers(DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker,
                        const Score moveScore, const int moveNumber);

    GameResult checkAdj(Match &match, const DrawAdjTracker &drawTracker,
                        const ResignAdjTracker &resignTracker, const Score score,
                        const Color lastSideThatMoved) const;

    bool checkEngineStatus(UciEngine &engine, Match &match, int roundId) const;
};

} // namespace fast_chess