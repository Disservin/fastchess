#pragma once

#include "../options.hpp"

namespace fast_chess {
class Tournament {
   public:
    explicit Tournament(const CMD::GameManagerOptions &game_config);

    void loadConfig(const CMD::GameManagerOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);

   private:
    void validateEngines(const std::vector<EngineConfiguration> &configs);

    CMD::GameManagerOptions game_config_ = {};

    size_t engine_count_ = 0;
};

}  // namespace fast_chess