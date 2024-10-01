#include <matchmaking/tournament/tournament_manager.hpp>

#include <printing/printing.h>
#include <cli/cli.hpp>
#include <config/config.hpp>
#include <config/sanitize.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>
#include <util/logger/logger.hpp>
#include <util/rand.hpp>

namespace fastchess {

TournamentManager::TournamentManager() {}

void TournamentManager::start(int argc, char const* argv[]) {
    const auto options = cli::OptionsParser(argc, argv);

    config::TournamentConfig.setup([&options]() -> std::unique_ptr<config::Tournament> {
        auto cnf = options.getTournamentConfig();

        config::sanitize(cnf);

        return std::make_unique<config::Tournament>(cnf);
    });

    config::EngineConfigs.setup([&options]() -> std::unique_ptr<std::vector<EngineConfiguration>> {
        auto cnf = options.getEngineConfigs();

        config::sanitize(cnf);

        return std::make_unique<std::vector<EngineConfiguration>>(cnf);
    });

    Logger::setLevel(config::TournamentConfig.get().log.level);
    Logger::setCompress(config::TournamentConfig.get().log.compress);
    Logger::openFile(config::TournamentConfig.get().log.file);

    util::random::seed(config::TournamentConfig.get().seed);

    Logger::trace("Creating tournament...");
    auto round_robin = RoundRobin(options.getResults());

    Logger::trace("Starting tournament...");
    round_robin.start();
}
}  // namespace fastchess
