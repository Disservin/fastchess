#include <doctest/doctest.hpp>

#include <core/printing/printing.h>
#include <cli/cli.hpp>
#include <cli/cli_args.hpp>
#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <core/rand.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>

using namespace fastchess;

static std::string catch_output(std::function<void()> func) {
    std::ostringstream oss;
    std::streambuf* p_cout_streambuf = std::cout.rdbuf();
    std::cout.rdbuf(oss.rdbuf());

    CHECK_NOTHROW(func());

    std::cout.rdbuf(p_cout_streambuf);  // restore

    return oss.str();
}

TEST_SUITE("Start from config") {
    TEST_CASE("Config 1") {
        const auto args = cli::Args{
            "fastchess",
            "-config",
            "stats=false",
            "file=./app/tests/configs/config.json",
        };

        const auto oss = catch_output([&]() {
            try {
                TournamentManager tournament = TournamentManager();
                tournament.start(args);

                Logger::info("Finished match");
            } catch (const std::exception& e) {
                stopProcesses();
                atomic::stop = false;

                std::cerr << e.what() << std::endl;

                throw e;
            }

            stopProcesses();
            atomic::stop = false;
        });

        CHECK(str_utils::contains(oss, "Loading config file: ./app/tests/configs/config.json"));
        CHECK(str_utils::contains(oss,
                                  "Warning: No opening book specified! Consider using one, otherwise all games will be "
                                  "played from the starting position."));
        CHECK(str_utils::contains(
            oss, "Warning: Unknown opening format, 2. All games will be played from the starting position."));

        CHECK(str_utils::contains(oss, "Started game 1 of 10 (engine1 vs engine2)"));
        CHECK(str_utils::contains(oss, "Started game 2 of 10 (engine2 vs engine1)"));
        CHECK(str_utils::contains(oss, "Started game 3 of 10 (engine1 vs engine2)"));
        CHECK(str_utils::contains(oss, "Started game 4 of 10 (engine2 vs engine1)"));
        CHECK(str_utils::contains(oss, "Started game 5 of 10 (engine1 vs engine2)"));
        CHECK(str_utils::contains(oss, "Started game 6 of 10 (engine2 vs engine1)"));
        CHECK(str_utils::contains(oss, "Started game 7 of 10 (engine1 vs engine2)"));
        CHECK(str_utils::contains(oss, "Started game 8 of 10 (engine2 vs engine1)"));
        CHECK(str_utils::contains(oss, "Started game 9 of 10 (engine1 vs engine2)"));
        CHECK(str_utils::contains(oss, "Started game 10 of 10 (engine2 vs engine1)"));

        CHECK(str_utils::contains(oss, "Finished game 1 (engine1 vs engine2):"));
        CHECK(str_utils::contains(oss, "Finished game 2 (engine2 vs engine1):"));
        CHECK(str_utils::contains(oss, "Finished game 3 (engine1 vs engine2):"));
        CHECK(str_utils::contains(oss, "Finished game 4 (engine2 vs engine1):"));
        CHECK(str_utils::contains(oss, "Finished game 5 (engine1 vs engine2):"));
        CHECK(str_utils::contains(oss, "Finished game 6 (engine2 vs engine1):"));
        CHECK(str_utils::contains(oss, "Finished game 7 (engine1 vs engine2):"));
        CHECK(str_utils::contains(oss, "Finished game 8 (engine2 vs engine1):"));
        CHECK(str_utils::contains(oss, "Finished game 9 (engine1 vs engine2):"));
        CHECK(str_utils::contains(oss, "Finished game 10 (engine2 vs engine1):"));

        CHECK(str_utils::contains(oss, "Results of engine1 vs engine2 (5+0.01, NULL, 16MB):"));
        CHECK(str_utils::contains(oss, "Elo:"));
        CHECK(str_utils::contains(oss, "LOS:"));
        CHECK(str_utils::contains(oss, "Games: 10,"));
        CHECK(str_utils::contains(oss, "Ptnml(0-2):"));

        CHECK(str_utils::contains(oss, "Finished match"));
    }

    TEST_CASE("Check version is included") {
        CHECK(str_utils::startsWith(cli::OptionsParser::Version, "fastchess tests "));
    }
}