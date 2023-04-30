#include <matchmaking/tournament.hpp>

#include <logger.hpp>
#include <rand.hpp>
#include <third_party/chess.hpp>

namespace fast_chess {

Tournament::Tournament(const CMD::GameManagerOptions& game_config) : round_robin_(game_config) {
    loadConfig(game_config);
}

void Tournament::loadConfig(const CMD::GameManagerOptions& game_config) {
    this->game_config_ = game_config;

    if (game_config_.games > 2)
        throw std::runtime_error("Error: Exceeded -game limit! Must be less than 2");
}

void Tournament::validateEngines(const std::vector<EngineConfiguration>& configs) {
    if (configs.size() < 2) {
        throw std::runtime_error("Error: Need at least two engines to start!");
    }

    for (size_t i = 0; i < configs.size(); i++) {
        for (size_t j = 0; j < i; j++) {
            if (configs[i].name == configs[j].name) {
                throw std::runtime_error("Error: Engine with the same name are not allowed!: " +
                                         configs[i].name);
            }
        }
    }

    engine_count_ = configs.size();
}

void Tournament::start(const std::vector<EngineConfiguration>& engine_configs) {
    validateEngines(engine_configs);

    Logger::cout("Starting tournament...");

    round_robin_.start(engine_configs);

    Logger::cout("Finished tournament\nSaving results...");
}

}  // namespace fast_chess