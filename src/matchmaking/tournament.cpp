#include <matchmaking/tournament.hpp>

#include <logger.hpp>
#include <rand.hpp>

namespace fast_chess {

Tournament::Tournament(const cmd::TournamentOptions& game_config) : round_robin_(game_config) {
    loadConfig(game_config);
}

void Tournament::loadConfig(const cmd::TournamentOptions& game_config) {
    game_config_ = game_config;

    if (game_config_.games > 2) {
        // wrong config, lets try to fix it
        std::swap(game_config_.games, game_config_.rounds);

        if (game_config_.games > 2) {
            throw std::runtime_error("Error: Exceeded -game limit! Must be less than 2");
        }
    }

    if (game_config.report_penta && game_config.output == OutputType::CUTECHESS)
        game_config_.report_penta = false;

    round_robin_.setGameConfig(game_config_);
}

void Tournament::validateEngines(const std::vector<EngineConfiguration>& configs) {
    if (configs.size() < 2) {
        throw std::runtime_error("Error: Need at least two engines to start!");
    }

    for (std::size_t i = 0; i < configs.size(); i++) {
        for (std::size_t j = 0; j < i; j++) {
            if (configs[i].name == configs[j].name) {
                throw std::runtime_error("Error: Engine with the same name are not allowed!: " +
                                         configs[i].name);
            }

            if (!std::filesystem::exists(configs[i].cmd)) {
                throw std::runtime_error("Error; Engine not found: " + configs[i].cmd);
            }
        }
    }
}

void Tournament::start(const std::vector<EngineConfiguration>& engine_configs) {
    validateEngines(engine_configs);

    Logger::cout("Starting tournament...");

    round_robin_.start(engine_configs);

    Logger::cout("Finished tournament\nSaving results...");
}

}  // namespace fast_chess