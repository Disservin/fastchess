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

    /// @brief stop the tournament
    void stop();

    /// @brief
    /// @param first engine name
    /// @param second engine name
    void printElo(const std::string &first, const std::string &second);

    void startTournament(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief accessed by [engine name][engine name]
    /// @return
    std::map<std::string, std::map<std::string, Stats>> getResults() const
    {
        return results_;
    }

  private:
    std::string fetchNextFen();

    void writeToFile(const std::string &data);

    /// @brief MT safe result updating
    /// @param us
    /// @param them
    /// @param stats
    void updateStats(const std::string &us, const std::string &them, const Stats &stats);

    /// @brief launch a match between two engines given their configs and a fen
    /// @param configs
    /// @param fen
    /// @return
    bool launchMatch(const std::pair<EngineConfiguration, EngineConfiguration> &configs,
                     const std::string &fen);

    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    /// @brief tracks the engine results
    std::map<std::string, std::map<std::string, Stats>> results_;
    std::mutex results_mutex_;

    /// @brief contains all opening fens
    std::vector<std::string> opening_book_;

    /// @brief file to write pgn to
    std::ofstream file_;
    std::mutex file_mutex_;

    /// @brief current index for the opening book
    uint64_t fen_index_ = 0;

    // General stuff
    std::atomic_int round_count_ = 0;
    std::atomic_int total_count_ = 0;
    std::atomic_int timeouts_ = 0;
};

} // namespace fast_chess