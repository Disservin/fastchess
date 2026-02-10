#include <matchmaking/sprt/sprt.hpp>

#include <doctest/doctest.hpp>

#include <matchmaking/stats.hpp>

using namespace fastchess;

TEST_SUITE("SPRT") {
    TEST_CASE("normalized trinomial 1") {
        Stats stats(36433, 36027, 68692);
        SPRT sprt(0.05, 0.05, 0, 2, "normalized", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(0.92).epsilon(0.01));
    }

    TEST_CASE("normalized trinomial 2") {
        Stats stats(10871, 10650, 20431);
        SPRT sprt(0.05, 0.05, -1.75, 0.25, "normalized", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(2.30).epsilon(0.01));
    }

    TEST_CASE("normalized trinomial 3") {
        Stats stats(4250, 0, 0);
        SPRT sprt(0.05, 0.05, 0, 10, "normalized", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(120.56).epsilon(0.01));
    }

    TEST_CASE("logistic trinomial 1") {
        Stats stats(21404, 21184, 40708);
        SPRT sprt(0.05, 0.05, 0.5, 2.5, "logistic", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(-1.57).epsilon(0.01));
    }

    TEST_CASE("logistic trinomial 2") {
        Stats stats(57433, 57030, 106593);
        SPRT sprt(0.05, 0.05, 0, 2, "logistic", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(-2.59).epsilon(0.01));
    }

    TEST_CASE("bayesian trinomial 1") {
        Stats stats(68965, 68526, 128429);
        SPRT sprt(0.05, 0.05, 0, 2, "bayesian", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(-1.26).epsilon(0.01));
    }

    TEST_CASE("bayesian trinomial 2") {
        Stats stats(21629, 21484, 41111);
        SPRT sprt(0.05, 0.05, 0.5, 2.5, "bayesian", true);

        CHECK(sprt.getLLR(stats, false) == doctest::Approx(-1.13).epsilon(0.01));
    }

    TEST_CASE("normalized pentanomial 1") {
        Stats stats(365, 16618, 36029, 200, 16974, 390);
        SPRT sprt(0.05, 0.05, 0, 2, "normalized", true);

        CHECK(sprt.getLLR(stats, true) == doctest::Approx(2.25).epsilon(0.01));
    }

    TEST_CASE("normalized pentanomial 2") {
        Stats stats(127, 4883, 10311, 401, 5150, 104);
        SPRT sprt(0.05, 0.05, -1.75, 0.25, "normalized", true);

        CHECK(sprt.getLLR(stats, true) == doctest::Approx(3.01).epsilon(0.01));
    }

    TEST_CASE("normalized pentanomial 3") {
        Stats stats(0, 0, 0, 0, 0, 5550);
        SPRT sprt(0.05, 0.05, 0, 5, "normalized", true);

        CHECK(sprt.getLLR(stats, true) == doctest::Approx(111.82).epsilon(0.01));
    }

    TEST_CASE("logistic pentanomial 1") {
        Stats stats(223, 9863, 20279, 1000, 10037, 246);
        SPRT sprt(0.05, 0.05, 0.5, 2.5, "logistic", true);

        CHECK(sprt.getLLR(stats, true) == doctest::Approx(-3.07).epsilon(0.01));
    }

    TEST_CASE("logistic pentanomial 2") {
        Stats stats(871, 26175, 55003, 980, 26678, 821);
        SPRT sprt(0.05, 0.05, 0, 2, "logistic", true);

        CHECK(sprt.getLLR(stats, true) == doctest::Approx(-4.98).epsilon(0.01));
    }
}
