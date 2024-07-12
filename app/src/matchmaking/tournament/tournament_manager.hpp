#pragma once

#include <config/types.hpp>
#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

namespace fast_chess {

// Manages the tournament, currently wraps round robin but can be extended to support
// different tournament types
class TournamentManager {
   public:
    TournamentManager(const std::vector<EngineConfiguration> &engine_configs, const stats_map &results);

    ~TournamentManager() {
        Logger::trace("Finished tournament.");
        stop();
    }

    void start();
    void stop() { round_robin_->stop(); }

    [[nodiscard]] RoundRobin *roundRobin() { return round_robin_.get(); }

   private:
    std::vector<EngineConfiguration> engine_configs_;

    std::unique_ptr<RoundRobin> round_robin_;
};

}  // namespace fast_chess
