#include <matchmaking/elo/elo_wdl.hpp>

#include "doctest/doctest.hpp"

#include <types/stats.hpp>

using namespace fast_chess;

TEST_SUITE("Elo Calculation Tests") {
    TEST_CASE("Elo calculation") {
        Stats stats(0, 36, 14);
        EloWDL elo(stats);

        // CHECK(elo.diff(stats) == doctest::Approx(-315.34).epsilon(0.01));
        // CHECK(elo.error(stats) == doctest::Approx(95.43).epsilon(0.01));
    }

    TEST_CASE("Elo calculation 2") {
        Stats stats(859, 772, 1329);
        EloWDL elo(stats);

        // CHECK(elo.diff(stats) == doctest::Approx(10.21).epsilon(0.01));
        // CHECK(elo.error(stats) == doctest::Approx(9.28).epsilon(0.01));
    }

    TEST_CASE("Elo calculation 3") {
        Stats stats(1164, 1267, 3049);
        EloWDL elo(stats);

        // CHECK(elo.diff(stats) == doctest::Approx(-6.53).epsilon(0.01));
        // CHECK(elo.error(stats) == doctest::Approx(6.12).epsilon(0.01));
    }
}
