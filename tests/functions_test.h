#pragma once

#include "../src/helper.h"
#include "../src/options.h"

TEST_CASE("Testing the startsWith function")
{
    CHECK(startsWith("-engine", "-"));
    CHECK(startsWith("-engine", "") == false);
    CHECK(startsWith("-engine", "/-") == false);
    CHECK(startsWith("-engine", "e") == false);
}

TEST_CASE("Testing the contains function")
{
    CHECK(contains("-engine", "-"));
    CHECK(contains("-engine", "e"));
    CHECK(contains("info string depth 10", "depth"));
}
