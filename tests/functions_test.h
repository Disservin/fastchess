#pragma once

#include "../src/helper.h"
#include "../src/options.h"

TEST_CASE("Testing the CMD::Options::startsWith function")
{
    CHECK(CMD::Options::startsWith("-engine", "-"));
    CHECK(CMD::Options::startsWith("-engine", "") == false);
    CHECK(CMD::Options::startsWith("-engine", "/-") == false);
    CHECK(CMD::Options::startsWith("-engine", "e") == false);
}

TEST_CASE("Testing the CMD::Options::contains function")
{
    CHECK(CMD::Options::contains("-engine", "-"));
    CHECK(CMD::Options::contains("-engine", "e"));
    CHECK(CMD::Options::contains("info string depth 10", "depth"));
}
