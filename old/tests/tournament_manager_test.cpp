#include <core/globals/globals.hpp>
#include <matchmaking/tournament/tournament_manager.hpp>
#include <types/exception.hpp>

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
}
