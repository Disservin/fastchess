#pragma once

#include <atomic>
#include <fstream>
#include <optional>
#include <utility>
#include <vector>

#include "chess/board.hpp"
#include "engines/engine_config.hpp"
#include "engines/uci_engine.hpp"
#include "matchmaking/matchmaking_data.hpp"
#include "matchmaking/threadpool.hpp"
#include "options.hpp"
#include "sprt.hpp"

namespace fast_chess
{

/*
 * This is the main class to start engines matches.
 * Generally we always swap the engine order, the first engine is always
 * the positive engine, meaning all stats should be calculated from that view point.
 */
class Tournament
{
  public:
    // For testing purposes
    explicit Tournament(bool saveTime) : save_time_header_(saveTime)
    {
        file_.open("fast-chess.pgn", std::ios::app);
    };

    explicit Tournament(const CMD::GameManagerOptions &mc);

    void loadConfig(const CMD::GameManagerOptions &mc);

    Stats getStats() const;
    void setStats(const Stats &stats);

    std::vector<std::string> getPGNS() const;
    void setStorePGN(bool v);

    void printElo() const;

    void startTournament(const std::vector<EngineConfiguration> &configs);

    void stopPool();

    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    static const Score mate_score_ = 100'000;

  private:
    std::string fetchNextFen();

    void writeToFile(const std::string &data);

    MoveData parseEngineOutput(const Board &board, const std::vector<std::string> &output,
                               const std::string &move, int64_t measuredTime) const;

    bool checkEngineStatus(UciEngine &engine, MatchInfo &match, int roundId) const;

    void updateTrackers(DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker,
                        const Score moveScore, const int move_number) const;

    GameResult checkAdj(MatchInfo &match, const DrawAdjTracker &drawTracker,
                        const ResignAdjTracker &resignTracker, const Score score,
                        const Color lastSideThatMoved) const;

    std::vector<MatchInfo> runH2H(CMD::GameManagerOptions localMatchConfig,
                                  const std::vector<EngineConfiguration> &configs,
                                  const std::string &fen);

    CMD::GameManagerOptions match_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    std::vector<std::string> pgns_;
    std::vector<std::string> opening_book_;
    std::vector<std::string> engine_names_;

    std::ofstream file_;
    std::mutex file_mutex_;

    std::atomic_int wins_ = 0;
    std::atomic_int draws_ = 0;
    std::atomic_int losses_ = 0;
    std::atomic_int round_count_ = 0;
    std::atomic_int total_count_ = 0;
    std::atomic_int timeouts_ = 0;

    std::atomic_int penta_WW_ = 0;
    std::atomic_int penta_WD_ = 0;
    std::atomic_int penta_WL_ = 0;
    std::atomic_int penta_LD_ = 0;
    std::atomic_int penta_LL_ = 0;

    // Stats from "White" pov for white advantage
    std::atomic_int white_wins_ = 0;
    std::atomic_int white_losses_ = 0;

    size_t fen_index_ = 0;

    bool store_pgns_ = false;

    bool save_time_header_ = true;
};

} // namespace fast_chess