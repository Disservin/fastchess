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

    util::random::seed(config::TournamentConfig->seed);

    LOG_TRACE("Creating tournament...");
    auto round_robin = RoundRobin(options.getResults());

    LOG_INFO("Starting tournament...");
    round_robin.start();

    if (auto crc32 = round_robin.getPgnCRC32(); crc32) {
        Logger::print("Games writte to {} with CRC32: {}", config::TournamentConfig->pgn.file, *crc32);
    }
}
}  // namespace fastchess
