#include "doctest/doctest.hpp"
#include "helper.hpp"
#include "options.hpp"

using namespace fast_chess;

TEST_SUITE("Standalone Function Tests") {
    TEST_CASE("Testing the startsWith function") {
        CHECK(startsWith("-engine", "-"));
        CHECK(startsWith("-engine", "") == false);
        CHECK(startsWith("-engine", "/-") == false);
        CHECK(startsWith("-engine", "e") == false);
    }

    TEST_CASE("Testing the contains function") {
        CHECK(contains("-engine", "-"));
        CHECK(contains("-engine", "e"));
        CHECK(contains("info string depth 10", "depth"));
    }
}