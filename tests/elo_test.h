#pragma once

#include "doctest/doctest.h"

#include "../src/elo.h"

TEST_CASE("Elo calculation")
{
    Elo elo(0, 36, 14);

    CHECK(elo.getDiff(0, 36, 14) == doctest::Approx(-315.34).epsilon(0.01));
    CHECK(elo.getError(0, 36, 14) == doctest::Approx(95.43).epsilon(0.01));
}

TEST_CASE("Elo calculation 2")
{
    Elo elo(859, 772, 1329);

    CHECK(elo.getDiff(859, 772, 1329) == doctest::Approx(10.21).epsilon(0.01));
    CHECK(elo.getError(859, 772, 1329) == doctest::Approx(9.28).epsilon(0.01));
}

TEST_CASE("Elo calculation 3")
{
    Elo elo(1164, 1267, 3049);

    CHECK(elo.getDiff(1164, 1267, 3049) == doctest::Approx(-6.53).epsilon(0.01));
    CHECK(elo.getError(1164, 1267, 3049) == doctest::Approx(6.12).epsilon(0.01));
}