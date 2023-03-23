#pragma once

#include <map>
#include <mutex>
#include <string>

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

    /// @brief
    /// @param first engine name
    /// @param second engine name
    void printElo(const std::string &first, const std::string &second);

    void startTournament(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief Used to load results from json file.
    /// @param results
    void setResults(const std::map<std::string, std::map<std::string, Stats>> &results) {
        results_ = results;
    }

    /// @brief non MT access
    /// @return
    std::map<std::string, std::map<std::string, Stats>> getResults() const { return results_; }

#ifdef TESTS
    std::vector<std::string> getPgns() const { return pgns_; }

#endif

   private:
    bool runSprt(const std::vector<EngineConfiguration> &engine_configs);

    void validateConfig(const std::vector<EngineConfiguration> &configs);

    void createRoundRobin(const std::vector<EngineConfiguration> &engine_configs,
                          std::vector<std::future<bool>> &results);

    /// @brief MT access by [engine name][engine name]
    /// @return
    Stats getResults(const std::string &engine1, const std::string &engine2);

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

    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    /// @brief how many engines are playing
    int engine_count_ = 0;

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
    std::atomic_int match_count_ = 0;
    std::atomic_int round_count_ = 0;

    std::atomic_int total_count_ = 0;
    std::atomic_int timeouts_ = 0;

    std::vector<std::string> pgns_;
};

}  // namespace fast_chess