#include "doctest/doctest.h"

#include "../src/helper.h"
#include "../src/options.h"

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
