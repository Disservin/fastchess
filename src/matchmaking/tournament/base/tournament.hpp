#pragma once

#include <vector>

#include <types/tournament_options.hpp>
#include <matchmaking/result.hpp>
#include <affinity/affinity_manager.hpp>
#include <util/cache.hpp>
#include <engines/uci_engine.hpp>
#include <matchmaking/opening_book.h>
#include <util/threadpool.hpp>
#include <util/file_writer.hpp>
#include <util/logger.hpp>

namespace fast_chess {
class ITournament {
   public:
    ITournament(const cmd::TournamentOptions &config)
        : output_(getNewOutput(config.output)),
          tournament_options_(config),

          book_(config.opening.file, config.opening.format, config.opening.start) {
        auto filename = (config.pgn.file.empty() ? "fast-chess" : config.pgn.file);

        if (config.output == OutputType::FASTCHESS) {
            filename += ".pgn";
        }

        file_writer_.open(filename);

        pool_.resize(tournament_options_.concurrency);
    }

    virtual ~ITournament() = default;

    virtual void start(const std::vector<EngineConfiguration> &engine_configs) {
        Logger::log<Logger::Level::TRACE>("Starting...");

        cores_ = std::make_unique<affinity::AffinityManager>(tournament_options_.affinity,
                                                             getMaxAffinity(engine_configs));

        create(engine_configs);
    }

    /// @brief forces the tournament to stop
    virtual void stop() {
        Logger::log<Logger::Level::TRACE>("Stopped!");
        atomic::stop = true;
        pool_.kill();
    }

    [[nodiscard]] stats_map getResults() noexcept { return result_.getResults(); }
    void setResults(const stats_map &results) noexcept { result_.setResults(results); }

    void setGameConfig(const cmd::TournamentOptions &game_config) noexcept {
        tournament_options_ = game_config;
    }

   protected:
    /// @brief creates the matches
    /// @param engine_configs
    /// @param results
    virtual void create(const std::vector<EngineConfiguration> &engine_configs) = 0;

    std::unique_ptr<IOutput> output_;

    cmd::TournamentOptions tournament_options_;
    std::unique_ptr<affinity::AffinityManager> cores_;

    OpeningBook book_;

    FileWriter file_writer_                         = FileWriter();
    CachePool<UciEngine, std::string> engine_cache_ = CachePool<UciEngine, std::string>();
    Result result_                                  = Result();
    ThreadPool pool_                                = ThreadPool(1);

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
};

}  // namespace fast_chess
