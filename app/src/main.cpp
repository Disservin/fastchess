#include <cstdlib>
#include <thread>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <cli/cli_args.hpp>
#include <cli/sprt.hpp>
#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <core/rand.hpp>
#include <engine/compliance.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

namespace fastchess {
const char* version = "alpha 1.2.0 ";
}

using namespace fastchess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();

    if (argc >= 3 && std::string(argv[1]) == "--compliance") {
        setTerminalOutput();
        return !engine::compliant(argc, argv);
    }

    if (argc >= 2 && std::string(argv[1]) == "--sprt") {
        std::string commandLineStr = "";
        for (int i = 1; i < argc; i++) commandLineStr.append(argv[i]).append(" ");

        cli::calculateSprt(commandLineStr);
        return 0;
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
