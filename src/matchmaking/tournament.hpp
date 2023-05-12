#pragma once

#include <matchmaking/roundrobin.hpp>
#include <types.hpp>

namespace fast_chess {
class Tournament {
   public:
    explicit Tournament(const cmd::GameManagerOptions &game_config);

    void loadConfig(const cmd::GameManagerOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);
    void stop() { round_robin_.stop(); }

    [[nodiscard]] stats_map getResults() { return round_robin_.getResults(); }

    [[nodiscard]] RoundRobin *roundRobin() { return &round_robin_; }

   private:
    void validateEngines(const std::vector<EngineConfiguration> &configs);

    RoundRobin round_robin_;

    cmd::GameManagerOptions game_config_;

    std::size_t engine_count_;
};

}  // namespace fast_chess