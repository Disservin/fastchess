#pragma once

#include <vector>

#include <affinity/affinity_manager.hpp>
#include <core/filesystem/file_writer.hpp>
#include <core/globals/globals.hpp>
#include <core/memory/cache.hpp>
#include <core/rand.hpp>
#include <core/threading/threadpool.hpp>
#include <game/book/opening_book.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/scoreboard.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <matchmaking/stats.hpp>
#include <types/tournament.hpp>

#include <matchmaking/tournament/gauntlet/scheduler.hpp>
#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

namespace fastchess {

class Gauntlet : public RoundRobin {
   public:
    explicit Gauntlet(const stats_map& results) : RoundRobin(results) {
        const auto& config     = *config::TournamentConfig;
        const auto num_players = config::EngineConfigs->size();

        generator_ = std::make_unique<GauntletScheduler>(book_.get(), num_players, config.rounds, config.games,
                                                         config.gauntlet_seeds, match_count_);
    }
};

}  // namespace fastchess
