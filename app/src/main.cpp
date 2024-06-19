#include <thread>

#include <cli/cli.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

using namespace fast_chess;

int main(int argc, char const *argv[]) {
    setCtrlCHandler();

    auto options = cli::OptionsParser(argc, argv);

    {
        auto tour = TournamentManager(options.getGameOptions(), options.getEngineConfigs(), options.getResults());

        tour.start();
    }

    stopProcesses();

    return 0;
}
