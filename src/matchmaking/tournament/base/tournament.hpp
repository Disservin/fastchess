#pragma once

#include <types/tournament_options.hpp>
#include <matchmaking/result.hpp>
#include <affinity/affinity_manager.hpp>
#include <util/cache.hpp>
#include <engines/uci_engine.hpp>
#include <matchmaking/opening_book.h>
#include <util/threadpool.hpp>
#include <util/file_writer.hpp>

namespace fast_chess {
class ITournament {
   public:
    ITournament(const cmd::TournamentOptions &config)
        : tournament_options_(config),
          book_(config.opening.file, config.opening.format, config.opening.start) {}

    virtual ~ITournament() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;

    virtual void setResults(const stats_map &results) noexcept                     = 0;
    virtual void setGameConfig(const cmd::TournamentOptions &game_config) noexcept = 0;

   private:
    std::unique_ptr<IOutput> output_;

    const cmd::TournamentOptions &tournament_options_;
    std::unique_ptr<affinity::AffinityManager> cores_;

    OpeningBook book_;
    FileWriter file_writer_;

    CachePool<UciEngine, std::string> engine_cache_ = CachePool<UciEngine, std::string>();
    Result result_                                  = Result();
    ThreadPool pool_                                = ThreadPool(1);
};

}  // namespace fast_chess
