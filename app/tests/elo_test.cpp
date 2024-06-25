#include <elo/elo_pentanomial.hpp>
#include <elo/elo_wdl.hpp>

#include "doctest/doctest.hpp"

#include <types/stats.hpp>

using namespace fast_chess;

TEST_SUITE("Elo Model") {
    TEST_CASE("EloWDL 1") {
        Stats stats(76, 89, 123);
        elo::EloWDL elo(stats);

        CHECK(elo.nEloDiff() == doctest::Approx(-20.76).epsilon(0.01));
        CHECK(elo.nEloError() == doctest::Approx(40.13).epsilon(0.01));
        CHECK(elo.los() == "15.53 %");
    }

    TEST_CASE("EloWDL 2") {
        Stats stats(136, 96, 111);
        elo::EloWDL elo(stats);

        CHECK(elo.nEloDiff() == doctest::Approx(49.77).epsilon(0.01));
        CHECK(elo.nEloError() == doctest::Approx(36.77).epsilon(0.01));

        CHECK(elo.diff() == doctest::Approx(49.70).epsilon(0.01));
        CHECK(elo.error() == doctest::Approx(36.77).epsilon(0.01));
        CHECK(elo.los() == "99.60 %");
    }

    TEST_CASE("EloWDL 3") {
        Stats stats(34, 356, 0);
        elo::EloWDL elo(stats);

        CHECK(elo.nEloDiff() == doctest::Approx(-508.44).epsilon(0.01));
        CHECK(elo.nEloError() == doctest::Approx(34.48).epsilon(0.01));
    }

    TEST_CASE("EloPentanomial 1") {
        Stats stats(34, 54, 31, 32, 64, 75);
        elo::EloPentanomial elo(stats);

        CHECK(elo.nEloDiff() == doctest::Approx(57.94).epsilon(0.01));
        CHECK(elo.nEloError() == doctest::Approx(28.28).epsilon(0.01));

        CHECK(elo.diff() == doctest::Approx(55.58).epsilon(0.01));
        CHECK(elo.error() == doctest::Approx(27.65).epsilon(0.01));
        CHECK(elo.los() == "100.00 %");
    }

    TEST_CASE("EloPentanomial 2") {
        Stats stats(332, 433, 457, 41, 333, 334);
        elo::EloPentanomial elo(stats);

        CHECK(elo.nEloDiff() == doctest::Approx(-9.17).epsilon(0.01));
        CHECK(elo.nEloError() == doctest::Approx(10.96).epsilon(0.01));

        CHECK(elo.diff() == doctest::Approx(-8.64).epsilon(0.01));
        CHECK(elo.error() == doctest::Approx(10.33).epsilon(0.01));
        CHECK(elo.los() == "5.05 %");
    }

    TEST_CASE("EloPentanomial 3") {
        Stats stats(7895, 8757, 5485, 200, 568, 9999);
        elo::EloPentanomial elo(stats);

        CHECK(elo.nEloDiff() == doctest::Approx(-19.01).epsilon(0.01));
        CHECK(elo.nEloError() == doctest::Approx(2.65).epsilon(0.01));

        CHECK(elo.diff() == doctest::Approx(-21.04).epsilon(0.01));
        CHECK(elo.error() == doctest::Approx(2.95).epsilon(0.01));
    }
}
