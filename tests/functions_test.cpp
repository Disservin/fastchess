#include <helper.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Standalone Function Tests") {
    TEST_CASE("Testing the StrUtil::startsWith function") {
        CHECK(StrUtil::startsWith("-engine", "-"));
        CHECK(StrUtil::startsWith("-engine", "") == false);
        CHECK(StrUtil::startsWith("-engine", "/-") == false);
        CHECK(StrUtil::startsWith("-engine", "e") == false);
    }

    TEST_CASE("Testing the StrUtil::contains function") {
        CHECK(StrUtil::contains("-engine", "-"));
        CHECK(StrUtil::contains("-engine", "e"));
        CHECK(StrUtil::contains("info string depth 10", "depth"));
    }
}