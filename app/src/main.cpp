#include <cstdlib>
#include <thread>

#include <cli/cli.hpp>
#include <config/config.hpp>
#include <config/sanitize.hpp>
#include <globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>
#include <util/rand.hpp>

using namespace fast_chess;

int main(int argc, char const* argv[]) {
    setCtrlCHandler();

    try {
        auto options = cli::OptionsParser(argc, argv);

        config::TournamentConfig.setup([&options]() -> std::unique_ptr<config::Tournament> {
            auto cnf = options.getTournamentConfig();

            config::sanitize(cnf);

            return std::make_unique<config::Tournament>(cnf);
        });

        config::EngineConfigs.setup([&options]() -> std::unique_ptr<std::vector<EngineConfiguration>> {
            auto cnf = options.getEngineConfigs();

            config::sanitize(cnf);

            return std::make_unique<std::vector<EngineConfiguration>>(cnf);
        });

        util::random::seed(config::TournamentConfig.get().seed);

        {
            auto tour = TournamentManager(options.getResults());

            tour.start();
        }

        stopProcesses();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    Logger::info("Finished match");

    return 0;
}
