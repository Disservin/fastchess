#pragma once

#include "../options.hpp"
#include "roundrobin.hpp"

namespace fast_chess {
class Tournament {
   public:
    explicit Tournament(const CMD::GameManagerOptions &game_config);

    void loadConfig(const CMD::GameManagerOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);
    void stop() { round_robin_.stop(); }

    stats_map getResults() { return round_robin_.getResults(); }

   private:
    void validateEngines(const std::vector<EngineConfiguration> &configs);

    RoundRobin round_robin_;

    CMD::GameManagerOptions game_config_ = {};

    size_t engine_count_ = 0;
};

}  // namespace fast_chess