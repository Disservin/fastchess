#include <matchmaking/tournament/tournament.hpp>

#include <util/logger.hpp>

namespace fast_chess {

Tournament::Tournament(const cmd::TournamentOptions& game_config,
                       const std::vector<EngineConfiguration>& engine_configs)
    : engine_configs_(engine_configs), tournament_options_(game_config), round_robin_(game_config) {
    validateEngines();
    fixConfig();
}

void Tournament::start() {
    Logger::log<Logger::Level::INFO>("Starting tournament...");

    round_robin_.start(engine_configs_);
}

void Tournament::fixConfig() {
    if (tournament_options_.games > 2) {
        // wrong config, lets try to fix it
        std::swap(tournament_options_.games, tournament_options_.rounds);

        if (tournament_options_.games > 2) {
            throw std::runtime_error("Error: Exceeded -game limit! Must be less than 2");
        }
    }

    // fix wrong config
    if (tournament_options_.report_penta && tournament_options_.output == OutputType::CUTECHESS)
        tournament_options_.report_penta = false;

    if (tournament_options_.opening.file.empty()) {
        Logger::log<Logger::Level::WARN>(
            "Warning: No opening book specified! Consider using one, otherwise all games will be "
            "played from the starting position.");
    }

    if (tournament_options_.opening.format != FormatType::EPD &&
        tournament_options_.opening.format != FormatType::PGN) {
        Logger::log<Logger::Level::WARN>(
            "Warning: Unknown opening format, " +
                std::to_string(int(tournament_options_.opening.format)) + ".",
            "All games will be played from the starting position.");
    }

    // update with fixed config
    round_robin_.setGameConfig(tournament_options_);
}

void Tournament::validateEngines() const {
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