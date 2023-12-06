#include <thread>

#include <cli/cli.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament.hpp>

using namespace fast_chess;

int main(int argc, char const *argv[]) {
    setCtrlCHandler();

    Logger::log<Logger::Level::TRACE>("Reading options...");
    auto options = cli::OptionsParser(argc, argv);

    {
        Logger::log<Logger::Level::TRACE>("Creating tournament...");
        auto tour = TournamentManager(options.getGameOptions(), options.getEngineConfigs());

        Logger::log<Logger::Level::TRACE>("Setting results...");
        tour.roundRobin()->setResults(options.getResults());

        Logger::log<Logger::Level::TRACE>("Starting tournament...");
        tour.start();

        Logger::log<Logger::Level::INFO>("Finished tournament.");
    }

    stopProcesses();

    return 0;
}
