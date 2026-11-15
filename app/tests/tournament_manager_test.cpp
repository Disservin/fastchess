#include <core/config/config.hpp>
#include <core/globals/globals.hpp>
#include <matchmaking/tournament/roundrobin/roundrobin.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>
#include <types/exception.hpp>

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.hpp>


using namespace fastchess;

TEST_SUITE("Tournament Manager Tests") {
    TEST_CASE("Should throw failed to load TBs") {
        const auto args = cli::Args{"fastchess.exe",
                                    "-engine",
                                    "cmd=app/tests/mock/engine/dummy_engine",
                                    "tc=40/40+1",
                                    "name=engine_1",
                                    "-engine",
                                    "cmd=app/tests/mock/engine/dummy_engine",
                                    "tc=40/40+1",
                                    "name=engine_2",
                                    "-tb",
                                    "path_does_not_exist"};

        const bool atomicStopBackup = atomic::stop;

        {
            TournamentManager tm;

            CHECK_THROWS_WITH_AS(
                tm.start(args),
                "Error: Failed to load Syzygy tablebases from the following directories: path_does_not_exist",
                fastchess_exception);
        }

        // ~TournamentManager modifies atomic::stop which pollutes the global state for subsequent tests.
        // Revert that modification manually.
        atomic::stop = atomicStopBackup;
    }

    TEST_CASE("Destruct without TBs") {
        const bool atomicStopBackup = atomic::stop;

        // Verify that running the destructor without having loaded TBs does not cause issues.
        {
            TournamentManager tm;
        }

        // ~TournamentManager modifies atomic::stop which pollutes the global state for subsequent tests.
        // Revert that modification manually.
        atomic::stop = atomicStopBackup;
    }

    TEST_CASE("Results are saved when a name is not valid UTF-8") {
        const auto file = std::string("tmp_invalid_utf8_config.json");

        auto tournamentBackup = std::move(config::TournamentConfig);
        auto enginesBackup    = std::move(config::EngineConfigs);

        config::TournamentConfig              = std::make_unique<config::Tournament>();
        config::TournamentConfig->config_name = file;
        config::EngineConfigs                 = std::make_unique<std::vector<EngineConfiguration>>(2);
        (*config::EngineConfigs)[0].name      = "engine_\xff";
        (*config::EngineConfigs)[1].name      = "engine_2";

        // The destructor saves the results; a strict dump threw out of it, which ended the program.
        {
            RoundRobin tournament(stats_map{});
        }

        std::ifstream in(file);
        std::stringstream content;
        content << in.rdbuf();
        in.close();
        std::remove(file.c_str());

        config::TournamentConfig = std::move(tournamentBackup);
        config::EngineConfigs    = std::move(enginesBackup);

        CHECK(content.str().find("engine_\xef\xbf\xbd") != std::string::npos);
        CHECK(content.str().find("engine_2") != std::string::npos);
    }
}
