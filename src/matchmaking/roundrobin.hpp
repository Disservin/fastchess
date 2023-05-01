#pragma once

#include <matchmaking/file_writer.hpp>
#include <matchmaking/match.hpp>
#include <matchmaking/result.hpp>
#include <matchmaking/threadpool.hpp>
#include <matchmaking/types/stats.hpp>
#include <options.hpp>
#include <sprt.hpp>

namespace fast_chess {

namespace Atomic {
extern std::atomic_bool stop;
}  // namespace Atomic

class RoundRobin {
   public:
    RoundRobin(const CMD::GameManagerOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);

    void stop() {
        Atomic::stop = true;
        Logger::cout("Stopped round robin!");
    }

    [[nodiscard]] stats_map getResults() { return result_.getResults(); }
    void setResults(const stats_map &results) { result_.setResults(results); }

   private:
    void setupOpeningBook();

    void create(const std::vector<EngineConfiguration> &engine_configs,
                std::vector<std::future<void>> &results);

    [[nodiscard]] bool sprt(const std::vector<EngineConfiguration> &engine_configs);

    void createPairings(const EngineConfiguration &player1, const EngineConfiguration &player2,
                        int current);

    [[nodiscard]] std::tuple<bool, Stats, std::string> playGame(
        const std::pair<EngineConfiguration, EngineConfiguration> &configs, const std::string &fen,
        int round_id);

    /// @brief create the Stats object from the match data
    /// @param match_data
    /// @return
    [[nodiscard]] Stats updateStats(const MatchData &match_data);

    /// @brief fetches the next fen from a sequential read opening book or from a randomized
    /// opening book order
    /// @return
    [[nodiscard]] std::string fetchNextFen();

    std::unique_ptr<Output> output_;

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    Result result_ = Result();

    FileWriter file_writer_;

    /// @brief StrUtil::contains all opening fens
    std::vector<std::string> opening_book_;

    std::atomic<uint64_t> match_count_ = 0;
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fast_chess
