#pragma once

#include <matchmaking/tournament/roundrobin/roundrobin.hpp>
#include <types/tournament.hpp>

namespace fast_chess {

// Manages the tournament, currently wraps round robin but can be extended to support
// different tournament types
class TournamentManager {
   public:
    TournamentManager(const stats_map &results);

    ~TournamentManager() {
        Logger::trace("Finished tournament.");
        stop();
    }

    void start();
    void stop() { round_robin_->stop(); }

    [[nodiscard]] RoundRobin *roundRobin() { return round_robin_.get(); }

   private:
    std::unique_ptr<RoundRobin> round_robin_;
};

}  // namespace fast_chess
