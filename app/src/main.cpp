#include <cstdlib>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <cli/cli_args.hpp>
#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <core/rand.hpp>
#include <engine/compliance.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

namespace fastchess {
const char* version = "alpha 1.7.0 ";
}

namespace ch = std::chrono;
using namespace fastchess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();

    if (argc >= 3 && std::string(argv[1]) == "--compliance") {
        setTerminalOutput();
        return !engine::compliant(argc, argv);
    }

    const auto t0 = ch::steady_clock::now();

    try {
        auto tournament = TournamentManager();
        tournament.start(cli::Args(argc, argv));

        if (atomic::abnormal_termination) {
            if (argc > 0 && config::TournamentConfig) {
                Logger::print("Tournament was interrupted. To resume the tournament, run: {} -config file={}", argv[0],
                              config::TournamentConfig->config_name);
            } else {
                Logger::print("Tournament was interrupted.");
            }
        }
    } catch (const fastchess_exception& e) {
        stopProcesses();

        Logger::print("{}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        stopProcesses();

        Logger::print(
            "PLEASE submit a bug report to https://github.com/Disservin/fastchess/issues/ and include command line "
            "parameters and possibly the stdout/log of fastchess.");
        Logger::print("{}", e.what());

        return EXIT_FAILURE;
    }

    stopProcesses();

    Logger::print("Finished match");

    const auto duration = ch::steady_clock::now() - t0;
    const auto h        = ch::duration_cast<ch::hours>(duration).count();
    const auto m        = ch::duration_cast<ch::minutes>(duration % ch::hours(1)).count();
    const auto s        = ch::duration_cast<ch::seconds>(duration % ch::minutes(1)).count();

    Logger::print("Total Time: {:02}:{:02}:{:02} (hours:minutes:seconds)\n", h, m, s);

    return atomic::abnormal_termination ? EXIT_FAILURE : EXIT_SUCCESS;
}
