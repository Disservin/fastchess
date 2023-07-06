#include <helper.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Standalone Function Tests") {
    TEST_CASE("Testing the str_utils::startsWith function") {
        CHECK(str_utils::startsWith("-engine", "-"));
        CHECK(str_utils::startsWith("-engine", "") == false);
        CHECK(str_utils::startsWith("-engine", "/-") == false);
        CHECK(str_utils::startsWith("-engine", "e") == false);
    }

    TEST_CASE("Testing the str_utils::contains function") {
        CHECK(str_utils::contains("-engine", "-"));
        CHECK(str_utils::contains("-engine", "e"));
        CHECK(str_utils::contains("info string depth 10", "depth"));
    }
}