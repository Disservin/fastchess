#include <thread>

#include <cli/cli.hpp>
#include <config/sanitize.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

using namespace fast_chess;

int main(int argc, char const *argv[]) {
    setCtrlCHandler();

    auto options = cli::OptionsParser(argc, argv);

    auto config         = options.getGameOptions();
    auto engine_configs = options.getEngineConfigs();

    config::sanitize(config);
    config::sanitize(engine_configs);

    {
        auto tour = TournamentManager(config, engine_configs, options.getResults());

        tour.start();
    }

    stopProcesses();

    Logger::info("Finished match");

    return 0;
}
