#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../src/helper.h"
#include "../src/options.h"

TEST_CASE("Testing the getUserInput function")
{
    const char *args[] = {"fastchess", "hello", "world"};

    CMD::Options options = CMD::Options(3, args);

    CHECK(options.getUserInput().size() == 2);
}

TEST_CASE("Testing the starts_with function")
{
    CHECK(starts_with("-engine", "-"));
    CHECK(starts_with("-engine", "") == false);
    CHECK(starts_with("-engine", "/-") == false);
    CHECK(starts_with("-engine", "e") == false);
}

TEST_CASE("Testing the contains function")
{
    CHECK(contains("-engine", "-"));
    CHECK(contains("-engine", "e"));
    CHECK(contains("info string depth 10", "depth"));
}