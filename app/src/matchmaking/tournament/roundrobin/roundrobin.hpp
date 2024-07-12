#pragma once

#include <affinity/affinity_manager.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/result.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <pgn/pgn_reader.hpp>
#include <types/stats.hpp>
#include <types/tournament.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/rand.hpp>
#include <util/threadpool.hpp>

#include <matchmaking/tournament/base/tournament.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class RoundRobin : public BaseTournament {
   public:
    explicit RoundRobin(const stats_map &results);

    // starts the round robin
    void start() override;

   protected:
    // creates the matches
    void create() override;

   private:
    // update the current running sprt. SPRT Config has to be valid.
    void updateSprtStatus(const std::vector<EngineConfiguration> &engine_configs,
                          const std::pair<const engine::UciEngine &, const engine::UciEngine &> &engines);

    SPRT sprt_ = SPRT();

    // number of games to be played
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fast_chess
