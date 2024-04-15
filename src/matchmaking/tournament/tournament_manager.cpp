#include <matchmaking/tournament/tournament_manager.hpp>

#include <util/logger/logger.hpp>

namespace fast_chess {

TournamentManager::TournamentManager(const options::Tournament& tournament_config,
                                     const std::vector<EngineConfiguration>& engine_configs)
    : engine_configs_(engine_configs),
      tournament_options_(tournament_config),
      round_robin_(fixConfig(tournament_options_), engine_configs_) {
    validateEngines();

    // Set the seed for the random number generator
    random::mersenne_rand.seed(tournament_options_.seed);
}

void TournamentManager::start() {
    Logger::log<Logger::Level::INFO>("Starting tournament...");

    round_robin_.start();
}

options::Tournament TournamentManager::fixConfig(options::Tournament config) {
    if (config.games > 2) {
        // wrong config, lets try to fix it
        std::swap(config.games, config.rounds);

        if (config.games > 2) {
            throw std::runtime_error("Error: Exceeded -game limit! Must be less than 2");
        }
    }

    // fix wrong config
    if (config.report_penta && config.output == OutputType::CUTECHESS) config.report_penta = false;

    if (config.report_penta && config.games != 2) config.report_penta = false;

    if (config.opening.file.empty()) {
        Logger::log<Logger::Level::WARN>(
            "Warning: No opening book specified! Consider using one, otherwise all games will be "
            "played from the starting position.");
    }

    if (config.opening.format != FormatType::EPD && config.opening.format != FormatType::PGN) {
        Logger::log<Logger::Level::WARN>(
            "Warning: Unknown opening format, " + std::to_string(int(config.opening.format)) + ".",
            "All games will be played from the starting position.");
    }

    return config;
}

void TournamentManager::validateEngines() const {
    if (engine_configs_.size() < 2) {
        throw std::runtime_error("Error: Need at least two engines to start!");
    }

    for (std::size_t i = 0; i < engine_configs_.size(); i++) {
        for (std::size_t j = 0; j < i; j++) {
            if (engine_configs_[i].name == engine_configs_[j].name) {
                throw std::runtime_error("Error: Engine with the same name are not allowed!: " +
                                         engine_configs_[i].name);
            }
        }
    }
}

}  // namespace fast_chess
