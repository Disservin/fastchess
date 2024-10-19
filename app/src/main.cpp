#include <cstdlib>
#include <thread>

#include <printing/printing.h>
#include <cli/cli.hpp>
#include <config/config.hpp>
#include <engine/compliance.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>
#include <util/rand.hpp>

using namespace fastchess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();
    setTerminalOutput();

    if (argc >= 3 && std::string(argv[1]) == "--compliance") {

        return !engine::compliant(argc, argv);
    }

    try {
        auto tournament = TournamentManager();
        tournament.start(argc, argv);
    } catch (const std::exception& e) {
        stopProcesses();

        std::cerr << e.what() << std::endl;

        return EXIT_FAILURE;
    }

    stopProcesses();

    Logger::info("Finished match");

    return 0;
}
