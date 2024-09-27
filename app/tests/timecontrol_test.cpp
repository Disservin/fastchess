#include <time/timecontrol.hpp>

#include "doctest/doctest.hpp"

using namespace fastchess;

TEST_SUITE("TimeControl") {
    TEST_CASE("moves/time+increment") {
        TimeControl::Limits limits;
        limits.moves = 3;
        limits.time = 10000;
        limits.increment = 100;
        limits.timemargin = 100;

        TimeControl tc(limits);

        CHECK(tc.updateTime(5555) == true);
        CHECK(tc.getTimeLeft() == tc.time + tc.increment - 5555 + tc.increment);
        CHECK(tc.getMovesLeft() == 2);

        CHECK(tc.updateTime(4745) == true);
        CHECK(tc.getTimeLeft() == tc.increment);
        CHECK(tc.getMovesLeft() == 1);

        CHECK(tc.updateTime(50) == true);
        CHECK(tc.getTimeLeft() == tc.time + tc.increment - 50 + tc.increment);
        CHECK(tc.getMovesLeft() == 3);

        CHECK(tc.updateTime(10251) == false);
        CHECK(tc.getTimeLeft() == tc.time + tc.increment - 10251);
        CHECK(tc.getMovesLeft() == 2);
    }

    TEST_CASE("Fixed time") {
        TimeControl::Limits limits;
        limits.fixed_time = 5000;
        limits.timemargin = 200;

        TimeControl tc(limits);


        CHECK(tc.updateTime(tc.fixed_time + tc.timemargin - 1) == true);
        CHECK(tc.getTimeLeft() == tc.fixed_time);
        
        CHECK(tc.updateTime(tc.fixed_time + tc.timemargin) == true);
        CHECK(tc.getTimeLeft() == tc.fixed_time);

        CHECK(tc.updateTime(tc.fixed_time + tc.timemargin + 1) == false);
        CHECK(tc.getTimeLeft() == tc.fixed_time - (tc.fixed_time + tc.timemargin + 1));
    }

    TEST_CASE("Fixed depth/nodes") {
        TimeControl::Limits limits;

        TimeControl tc(limits);

        CHECK(tc.updateTime(523199) == true);
        CHECK(tc.getTimeLeft() == 0);
    }
}
