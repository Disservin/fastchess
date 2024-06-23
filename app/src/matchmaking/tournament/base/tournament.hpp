#pragma once

#include <vector>

#include <affinity/affinity_manager.hpp>
#include <book/opening_book.hpp>
#include <engine/uci_engine.hpp>
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
    BaseTournament(const options::Tournament &config, const std::vector<EngineConfiguration> &engine_configs,
                   const stats_map &results);

    virtual ~BaseTournament() {
        Logger::trace("Destroying tournament...");
        saveJson();
    }

    virtual void start();

    // forces the tournament to stop
    virtual void stop();

    [[nodiscard]] stats_map getResults() noexcept { return result_.getResults(); }
    void setResults(const stats_map &results) noexcept {
        Logger::trace("Setting results...");

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
    }

    void setGameConfig(const options::Tournament &tournament_config) noexcept {
        tournament_options_ = tournament_config;
    }

   protected:
    // number of games played
    std::atomic<std::uint64_t> match_count_;
    std::uint64_t initial_matchcount_;

    // creates the matches
    virtual void create() = 0;

    using start_callback    = std::function<void()>;
    using finished_callback = std::function<void(const Stats &stats, const std::string &reason, const engines &)>;

    // Function to save the config file
    void saveJson();

    // play one game and write it to the pgn file
    void playGame(const std::pair<EngineConfiguration, EngineConfiguration> &configs, start_callback start,
                  finished_callback finish, const pgn::Opening &opening, std::size_t game_id);

    std::unique_ptr<IOutput> output_;
    std::unique_ptr<affinity::AffinityManager> cores_;
    std::unique_ptr<util::FileWriter> file_writer_pgn;
    std::unique_ptr<util::FileWriter> file_writer_epd;
    std::unique_ptr<book::OpeningBook> book_;

    options::Tournament tournament_options_;
    std::vector<EngineConfiguration> engine_configs_;

    util::CachePool<engine::UciEngine, std::string> engine_cache_ = util::CachePool<engine::UciEngine, std::string>();
    Result result_                                                = Result();
    util::ThreadPool pool_                                        = util::ThreadPool(1);

   private:
    int getMaxAffinity(const std::vector<EngineConfiguration> &configs) const noexcept;
};

}  // namespace fast_chess
