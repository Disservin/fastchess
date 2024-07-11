#pragma once

#include <matchmaking/tournament/roundrobin/roundrobin.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess {

// Manages the tournament, currently wraps round robin but can be extended to support
// different tournament types
class TournamentManager {
   public:
    TournamentManager(const options::Tournament &game_config, const std::vector<EngineConfiguration> &engine_configs,
                      const stats_map &results);

    ~TournamentManager() {
        Logger::info("Finished match");
        stop();
    }

    void start();
    void stop() { round_robin_->stop(); }

    [[nodiscard]] RoundRobin *roundRobin() { return round_robin_.get(); }

   private:
    options::Tournament fixConfig(options::Tournament config);
    void validateEngines() const;

    std::vector<EngineConfiguration> engine_configs_;
    options::Tournament tournament_options_;

    std::unique_ptr<RoundRobin> round_robin_;
};

}  // namespace fast_chess
