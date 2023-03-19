#pragma once

#include <map>
#include <mutex>
#include <string>

#include "matchmaking/threadpool.hpp"
#include "options.hpp"
#include "sprt.hpp"

namespace fast_chess
{

struct Stats
{
    int wins = 0;
    int losses = 0;
    int draws = 0;

    int penta_WW = 0;
    int penta_WD = 0;
    int penta_WL = 0;
    int penta_LD = 0;
    int penta_LL = 0;

    Stats &operator+=(const Stats &rhs)
    {

        this->wins += rhs.wins;
        this->losses += rhs.losses;
        this->draws += rhs.draws;

        this->penta_WW += rhs.penta_WW;
        this->penta_WD += rhs.penta_WD;
        this->penta_WL += rhs.penta_WL;
        this->penta_LD += rhs.penta_LD;
        this->penta_LL += rhs.penta_LL;
        return *this;
    }
};

class Tournament
{

  public:
    explicit Tournament(bool savetime)
    {
        if (savetime)
            file_.open("fast-chess.pgn", std::ios::app);
    };

    explicit Tournament(const CMD::GameManagerOptions &game_config);

    void loadConfig(const CMD::GameManagerOptions &game_config);

    void stop();

    void printElo(const std::string &first, const std::string &second);

    void startTournament(const std::vector<EngineConfiguration> &engine_configs);

    std::map<std::string, std::map<std::string, Stats>> getResults() const
    {
        return results_;
    }

  private:
    std::string fetchNextFen();

    void writeToFile(const std::string &data);

    void updateStats(const std::string &us, const std::string &them, const Stats &stats);

    bool launchMatch(const std::pair<EngineConfiguration, EngineConfiguration> &configs,
                     const std::string &fen);

    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    std::mutex results_mutex_;
    std::map<std::string, std::map<std::string, Stats>> results_;

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    std::vector<std::string> opening_book_;

    std::ofstream file_;
    std::mutex file_mutex_;

    uint64_t fen_index_ = 0;

    std::atomic_int round_count_ = 0;
    std::atomic_int total_count_ = 0;
    std::atomic_int timeouts_ = 0;
};

} // namespace fast_chess