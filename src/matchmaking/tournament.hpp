#pragma once

#include <matchmaking/roundrobin.hpp>

#include <tournament_options.hpp>

namespace fast_chess {

/// @brief Manages the tournament, currenlty wraps round robin but can be extended to support
/// different tournament types
class Tournament {
   public:
    explicit Tournament(const cmd::TournamentOptions &game_config);

    void loadConfig(const cmd::TournamentOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);
    void stop() { round_robin_.stop(); }

    [[nodiscard]] stats_map getResults() { return round_robin_.getResults(); }

    [[nodiscard]] RoundRobin *roundRobin() { return &round_robin_; }

   private:
    void validateEngines(const std::vector<EngineConfiguration> &configs);

    RoundRobin round_robin_;

    cmd::TournamentOptions game_config_;
};

}  // namespace fast_chess