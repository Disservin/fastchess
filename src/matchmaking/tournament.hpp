#pragma once

#include <map>
#include <mutex>
#include <string>

#include "matchmaking/output.hpp"
#include "matchmaking/threadpool.hpp"
#include "matchmaking/tournament_data.hpp"
#include "options.hpp"
#include "sprt.hpp"
namespace fast_chess {

class Tournament {
   public:
    explicit Tournament(const CMD::GameManagerOptions &game_config);

    void loadConfig(const CMD::GameManagerOptions &game_config);

    /// @brief stop the tournament
    void stop();

    void startTournament(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief Used to load results from json file.
    /// @param results
    void setResults(const std::map<std::string, std::map<std::string, Stats>> &results) {
        results_ = results;
    }

    /// @brief non MT access
    /// @return
    std::map<std::string, std::map<std::string, Stats>> results() const { return results_; }

#ifdef TESTS
    std::vector<std::string> pgns() const { return pgns_; }
#endif

    int engineCount() const { return engine_count_; }

    CMD::GameManagerOptions gameConfig() const { return game_config_; }

    int timeouts() const { return timeouts_; }

    /// @brief tracks the engine results
    std::map<std::string, std::map<std::string, Stats>> results_;
    std::mutex results_mutex_;

    int matchCount() const { return match_count_; }

    int totalCount() const { return total_count_; }

   private:
    bool runSprt(const std::vector<EngineConfiguration> &engine_configs);

    void validateConfig(const std::vector<EngineConfiguration> &configs);

    void createGames(int sum, const EngineConfiguration &player1,
                     const EngineConfiguration &player2,
                     std::vector<std::future<bool>> &results);

    void createRoundRobin(const std::vector<EngineConfiguration> &engine_configs,
                          std::vector<std::future<bool>> &results);

    /// @brief MT access by [engine name][engine name]
    /// @return
    Stats results(const std::string &engine1, const std::string &engine2);

    /// @brief fetches the next fen from a sequential read opening book or from a randomized opening
    /// book order
    /// @return
    std::string fetchNextFen();

    /// @brief pgn writer
    /// @param data
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
                     const std::string &fen, int round_id);

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    /// @brief contains all opening fens
    std::vector<std::string> opening_book_;

    /// @brief collection of all pgns for the tournament
    std::vector<std::string> pgns_;

    std::unique_ptr<Output> output_ = nullptr;

    /// @brief file to write pgn to
    std::ofstream file_;
    std::mutex file_mutex_;

    // General stuff
    std::atomic_int match_count_ = 0;
    std::atomic_int round_count_ = 0;
    std::atomic_int total_count_ = 0;
    std::atomic_int timeouts_ = 0;

    /// @brief how many engines are playing
    int engine_count_ = 0;
};

}  // namespace fast_chess