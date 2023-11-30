#pragma once

#include <matchmaking/opening_book.h>
#include <affinity/affinity_manager.hpp>
#include <matchmaking/match.hpp>
#include <matchmaking/result.hpp>
#include <pgn_reader.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <types/stats.hpp>
#include <types/tournament_options.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/rand.hpp>
#include <util/threadpool.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class RoundRobin {
   public:
    explicit RoundRobin(const cmd::TournamentOptions &game_config);

    /// @brief starts the round robin
    /// @param engine_configs
    void start(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief forces the round robin to stop
    void stop() {
        atomic::stop = true;
        Logger::log<Logger::Level::TRACE>("Stopped round robin!");
        pool_.kill();
    }

    [[nodiscard]] stats_map getResults() noexcept { return result_.getResults(); }
    void setResults(const stats_map &results) noexcept { result_.setResults(results); }

    void setGameConfig(const cmd::TournamentOptions &game_config) noexcept {
        tournament_options_ = game_config;
    }

   private:
    int getMaxAffinity(const std::vector<EngineConfiguration> &configs) const noexcept {
        constexpr auto transform = [](const auto &val) { return std::stoi(val); };

        // const auto first_threads  = configs.first.getOption<int>("Threads",
        // transform).value_or(1); const auto second_threads =
        // configs.second.getOption<int>("Threads", transform).value_or(1);

        const auto first_threads = configs[0].getOption<int>("Threads", transform).value_or(1);

        for (const auto &config : configs) {
            const auto threads = config.getOption<int>("Threads", transform).value_or(1);

            // thread count in all configs has to be the same for affinity to work,
            // otherwise we set it to 0 and affinity is disabled
            if (threads != first_threads) {
                return 0;
            }
        }

        return first_threads;
    }

    /// @brief creates the matches
    /// @param engine_configs
    /// @param results
    void create(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief update the current running sprt. SPRT Config has to be valid.
    /// @param engine_configs
    void updateSprtStatus(const std::vector<EngineConfiguration> &engine_configs);

    using start_callback    = std::function<void()>;
    using finished_callback = std::function<void(const Stats &stats, const std::string &reason)>;

    /// @brief play one game and write it to the pgn file
    /// @param configs
    /// @param opening
    /// @param round_id
    /// @param start
    /// @param finish
    void playGame(const std::pair<EngineConfiguration, EngineConfiguration> &configs,
                  start_callback start, finished_callback finish, const Opening &opening,
                  std::size_t round_id);

    /// @brief Outputs the current state of the round robin to the console
    std::unique_ptr<IOutput> output_;

    cmd::TournamentOptions tournament_options_ = {};

    std::unique_ptr<affinity::AffinityManager> cores_;

    CachePool<UciEngine, std::string> engine_cache_ = CachePool<UciEngine, std::string>();

    /// @brief the file writer for the pgn file
    FileWriter file_writer_;

    ThreadPool pool_ = ThreadPool(1);

    Result result_ = Result();

    SPRT sprt_ = SPRT();

    OpeningBook book_;

    /// @brief number of games played
    std::atomic<uint64_t> match_count_ = 0;
    /// @brief number of games to be played
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fast_chess
