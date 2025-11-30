#include <core/helper.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess;

TEST_SUITE("Standalone Function Tests") {
    TEST_CASE("Testing the str_utils::startsWith function") {
        CHECK(str_utils::startsWith("-engine", "-"));
        CHECK(!str_utils::startsWith("-engine", ""));
        CHECK(!str_utils::startsWith("-engine", "/-"));
        CHECK(!str_utils::startsWith("-engine", "e"));
    }

    TEST_CASE("Testing the str_utils::contains function") {
        CHECK(str_utils::contains("-engine", "-"));
        CHECK(str_utils::contains("-engine", "e"));
        CHECK(str_utils::contains("info string depth 10", "depth"));
    }

    TEST_CASE("Testing the str_utils::findElement function") {
        const auto str = "bestmove ";

        CHECK(!str_utils::findElement<std::string>(str_utils::splitString(str, ' '), "bestmove").has_value());
    }
}