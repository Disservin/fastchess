#include <thread>

#include <cli/cli.hpp>
#include <config/config.hpp>
#include <config/sanitize.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

using namespace fast_chess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();

    auto options = cli::OptionsParser(argc, argv);

    config::Tournament.setup([&options]() -> std::unique_ptr<config::TournamentType> {
        auto cnf = options.getTournament();

        config::sanitize(cnf);

        return std::make_unique<config::TournamentType>(cnf);
    });

    config::EngineConfigs.setup([&options]() -> std::unique_ptr<std::vector<EngineConfiguration>> {
        auto cnf = options.getEngineConfigs();

        config::sanitize(cnf);

        return std::make_unique<std::vector<EngineConfiguration>>(cnf);
    });

    {
        auto tour = TournamentManager(options.getResults());

        tour.start();
    }

    stopProcesses();

    Logger::info("Finished match");

    return 0;
}
