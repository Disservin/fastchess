#include <time/timecontrol.hpp>

#include "doctest/doctest.hpp"

using namespace fastchess;

TEST_SUITE("Elo Model") {
    TEST_CASE("moves/time+increment") {
        TimeControl tc;
        tc.setMoves(3);
        tc.setTime(10000);
        tc.setIncrement(100);
        tc.setTimemargin(100);

        CHECK(tc.updateTime(5555) == true);
        CHECK(tc.getTimeLeft() == 4545);
        CHECK(tc.getMovesLeft() == 2);

        CHECK(tc.updateTime(4645) == true);
        CHECK(tc.getTimeLeft() == 100);
        CHECK(tc.getMovesLeft() == 1);

        CHECK(tc.updateTime(50) == true);
        CHECK(tc.getTimeLeft() == 10150);
        CHECK(tc.getMovesLeft() == 3);

        CHECK(tc.updateTime(10251) == false);
        CHECK(tc.getTimeLeft() == -1);
        CHECK(tc.getMovesLeft() == 2);
    }
}
