#include <thread>

#include <cli/cli.hpp>
#include <config/config.hpp>
#include <config/sanitize.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

using namespace fast_chess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();

    auto options        = cli::OptionsParser(argc, argv);
    auto engine_configs = options.getEngineConfigs();

    config::sanitize(engine_configs);

    config::Tournament.setup([&options]() -> std::unique_ptr<config::TournamentType> {
        auto cnf = options.getGameOptions();

        config::sanitize(cnf);

        return std::make_unique<config::TournamentType>(cnf);
    });

    {
        auto tour = TournamentManager(engine_configs, options.getResults());

        tour.start();
    }

    stopProcesses();

    Logger::info("Finished match");

    return 0;
}
