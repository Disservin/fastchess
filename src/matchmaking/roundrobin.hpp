#pragma once

#include <matchmaking/file_writer.hpp>
#include <matchmaking/match.hpp>
#include <matchmaking/result.hpp>
#include <matchmaking/threadpool.hpp>
#include <matchmaking/types/stats.hpp>
#include <pgn_reader.hpp>
#include <sprt.hpp>
#include <types.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class RoundRobin {
   public:
    explicit RoundRobin(const cmd::GameManagerOptions &game_config);

    /// @brief starts the round robin
    /// @param engine_configs
    void start(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief forces the round robin to stop
    void stop() {
        atomic::stop = true;
        pool_.kill();
        Logger::cout("Stopped round robin!");
    }

    [[nodiscard]] stats_map getResults() { return result_.getResults(); }
    void setResults(const stats_map &results) { result_.setResults(results); }

   private:
    /// @brief load a pgn opening book
    void setupPgnOpeningBook();
    /// @brief load a epd opening book
    void setupEpdOpeningBook();

    /// @brief creates the matches
    /// @param engine_configs
    /// @param results
    void create(const std::vector<EngineConfiguration> &engine_configs,
                std::vector<std::future<void>> &results);

    /// @brief run as a sprt test
    /// @param engine_configs
    /// @return
    [[nodiscard]] bool sprt(const std::vector<EngineConfiguration> &engine_configs);

    void updateSprtStatus(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief pairs player1 vs player2
    /// @param player1
    /// @param player2
    /// @param current
    void createPairings(const EngineConfiguration &player1, const EngineConfiguration &player2,
                        int current);

    /// @brief play one game and write it to the pgn file
    /// @param configs
    /// @param opening
    /// @param round_id
    /// @return
    [[nodiscard]] std::tuple<bool, Stats, std::string> playGame(
        const std::pair<EngineConfiguration, EngineConfiguration> &configs, const Opening &opening,
        int round_id);

    /// @brief create the Stats object from the match data
    /// @param match_data
    /// @return
    [[nodiscard]] static Stats updateStats(const MatchData &match_data);

    /// @brief fetches the next fen from a sequential read opening book or from a randomized
    /// opening book order
    /// @return
    [[nodiscard]] Opening fetchNextOpening();

    std::unique_ptr<IOutput> output_;

    cmd::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    Result result_ = Result();

    FileWriter file_writer_;

    std::vector<std::string> opening_book_epd;
    std::vector<Opening> opening_book_pgn;

    std::atomic<uint64_t> match_count_ = 0;
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fast_chess
