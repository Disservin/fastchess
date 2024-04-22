#pragma once

#include <vector>

#include <affinity/affinity_manager.hpp>
#include <engines/uci_engine.hpp>
#include <matchmaking/book/opening_book.hpp>
#include <matchmaking/output/output.hpp>
#include <matchmaking/result.hpp>
#include <types/tournament_options.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/logger/logger.hpp>
#include <util/threadpool.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class BaseTournament {
   public:
    BaseTournament(const options::Tournament &config,
                   const std::vector<EngineConfiguration> &engine_configs);

    virtual ~BaseTournament() = default;

    virtual void start();

    /// @brief forces the tournament to stop
    virtual void stop();

    [[nodiscard]] stats_map getResults() noexcept { return result_.getResults(); }
    void setResults(const stats_map &results) noexcept {
        result_.setResults(results);

        match_count_ = 0;

        for (const auto &pair1 : result_.getResults()) {
            const auto &inner_map = pair1.second;
            for (const auto &pair2 : inner_map) {
                const auto &stats = pair2.second;
                match_count_ += stats.wins + stats.losses + stats.draws;
            }
        }

        initial_matchcount_ = match_count_;

        book_.setInternalOffset(match_count_);
    }

    void setGameConfig(const options::Tournament &tournament_config) noexcept {
        tournament_options_ = tournament_config;
    }

   protected:
    /// @brief number of games played
    std::atomic<std::uint64_t> match_count_;
    std::uint64_t initial_matchcount_;

    /// @brief creates the matches
    virtual void create() = 0;

    using start_callback    = std::function<void()>;
    using finished_callback = std::function<void(const Stats &stats, const std::string &reason)>;

    /// @brief play one game and write it to the pgn file
    /// @param configs
    /// @param start
    /// @param finish
    /// @param opening
    /// @param game_id
    void playGame(const std::pair<EngineConfiguration, EngineConfiguration> &configs,
                  start_callback start, finished_callback finish, const Opening &opening,
                  std::size_t game_id);

    std::unique_ptr<IOutput> output_;
    std::unique_ptr<affinity::AffinityManager> cores_;
    std::unique_ptr<FileWriter> file_writer_;

    OpeningBook book_;
    options::Tournament tournament_options_;
    std::vector<EngineConfiguration> engine_configs_;

    CachePool<UciEngine, std::string> engine_cache_ = CachePool<UciEngine, std::string>();
    Result result_                                  = Result();
    ThreadPool pool_                                = ThreadPool(1);

   private:
    int getMaxAffinity(const std::vector<EngineConfiguration> &configs) const noexcept;
};

}  // namespace fast_chess
