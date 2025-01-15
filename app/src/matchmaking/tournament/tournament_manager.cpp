#include <matchmaking/tournament/tournament_manager.hpp>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <cli/cli_args.hpp>
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

void TournamentManager::start(const cli::Args& args) {
    const auto options = cli::OptionsParser(args);

    config::TournamentConfig = std::make_unique<config::Tournament>(options.getTournamentConfig());

    config::EngineConfigs = std::make_unique<std::vector<EngineConfiguration>>(options.getEngineConfigs());

    Logger::setLevel(config::TournamentConfig->log.level);
    Logger::setCompress(config::TournamentConfig->log.compress);
    Logger::openFile(config::TournamentConfig->log.file);

    Logger::trace("{}", cli::OptionsParser::Version);

    util::random::seed(config::TournamentConfig->seed);

    Logger::trace("Creating tournament...");
    auto round_robin = RoundRobin(options.getResults());

    Logger::trace("Starting tournament...");
    round_robin.start();
}
}  // namespace fastchess
