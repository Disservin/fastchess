#include <matchmaking/tournament/tournament_manager.hpp>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <core/logger/logger.hpp>
#include <core/rand.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

namespace fastchess {

TournamentManager::TournamentManager() {}

TournamentManager::~TournamentManager() {
    atomic::stop = true;
    Logger::trace("~TournamentManager()");
}

void TournamentManager::start(int argc, char const* argv[]) {
    const auto options = cli::OptionsParser(argc, argv);

    config::TournamentConfig.setup([&options]() -> std::unique_ptr<config::Tournament> {
        return std::make_unique<config::Tournament>(options.getTournamentConfig());
    });

    config::EngineConfigs.setup([&options]() -> std::unique_ptr<std::vector<EngineConfiguration>> {
        return std::make_unique<std::vector<EngineConfiguration>>(options.getEngineConfigs());
    });

    Logger::setLevel(config::TournamentConfig.get().log.level);
    Logger::setCompress(config::TournamentConfig.get().log.compress);
    Logger::openFile(config::TournamentConfig.get().log.file);

    Logger::trace("{}", cli::OptionsParser::version);

    util::random::seed(config::TournamentConfig.get().seed);

    Logger::trace("Creating tournament...");
    auto round_robin = RoundRobin(options.getResults());

    Logger::trace("Starting tournament...");
    round_robin.start();
}
}  // namespace fastchess
