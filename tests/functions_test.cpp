#include "doctest/doctest.hpp"

#include "chess/helper.hpp"
#include "options.hpp"

using namespace fast_chess;

TEST_SUITE("Standalone Function Tests")
{
    TEST_CASE("Testing the CMD::startsWith function")
    {
        CHECK(CMD::startsWith("-engine", "-"));
        CHECK(CMD::startsWith("-engine", "") == false);
        CHECK(CMD::startsWith("-engine", "/-") == false);
        CHECK(CMD::startsWith("-engine", "e") == false);
    }

    TEST_CASE("Testing the CMD::contains function")
    {
        CHECK(CMD::contains("-engine", "-"));
        CHECK(CMD::contains("-engine", "e"));
        CHECK(CMD::contains("info string depth 10", "depth"));
    }
}