#include <matchmaking/tournament/tournament_manager.hpp>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <cli/cli_args.hpp>
#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <core/logger/logger.hpp>
#include <core/rand.hpp>
#include <matchmaking/syzygy.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

#include <stdexcept>

namespace fastchess {

TournamentManager::TournamentManager() {}

TournamentManager::~TournamentManager() {
    // Note: this is safe to call even if initSyzygy() was not called.
    tearDownSyzygy();

    atomic::stop = true;
    LOG_TRACE("~TournamentManager()");
}

void TournamentManager::start(const cli::Args& args) {
    const auto options = cli::OptionsParser(args);

    config::TournamentConfig = std::make_unique<config::Tournament>(options.getTournamentConfig());

    config::EngineConfigs = std::make_unique<std::vector<EngineConfiguration>>(options.getEngineConfigs());

    Logger::setLevel(config::TournamentConfig->log.level);
    Logger::setCompress(config::TournamentConfig->log.compress);
    Logger::openFile(config::TournamentConfig->log.file);
    Logger::setEngineComs(config::TournamentConfig->log.engine_coms);

    LOG_INFO("{}", cli::OptionsParser::Version);

    random::seed(config::TournamentConfig->seed);

    if (config::TournamentConfig->tb_adjudication.enabled) {
        LOG_INFO("Loading Syzygy tablebases...");
        const int tbPieces = initSyzygy(config::TournamentConfig->tb_adjudication.syzygy_dirs);
        if (tbPieces == 0) {
            throw std::runtime_error("Error: Failed to load Syzygy tablebases from the following directories: " +
                                     config::TournamentConfig->tb_adjudication.syzygy_dirs);
        }
        LOG_INFO("Loaded {}-piece Syzygy tablebases.", tbPieces);
    }

    LOG_TRACE("Creating tournament...");
    BaseTournament *tournament;
    if (config::TournamentConfig->type == TournamentType::ROUNDROBIN)
        tournament = new RoundRobin(options.getResults());
    else if (config::TournamentConfig->type == TournamentType::GAUNTLET)
        tournament = new Gauntlet(options.getResults());

    LOG_INFO("Starting tournament...");
    tournament->start();
}
}  // namespace fastchess
