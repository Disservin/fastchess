#include <cstdlib>
#include <thread>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <cli/cli_args.hpp>
#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <core/rand.hpp>
#include <engine/compliance.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

namespace fastchess {
const char* version = "alpha 1.1.0 ";
}

using namespace fastchess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();
    setTerminalOutput();

    if (argc >= 3 && std::string(argv[1]) == "--compliance") {
        return !engine::compliant(argc, argv);
    }

    try {
        auto tournament = TournamentManager();
        tournament.start(cli::Args(argc, argv));
    } catch (const std::exception& e) {
        stopProcesses();

        std::cerr << e.what() << std::endl;

        return EXIT_FAILURE;
    }

    stopProcesses();

    Logger::info("Finished match");

    return 0;
}
