#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../src/options.h"

TEST_CASE("Testing the options functions")
{
    const char *args[] = {"fastchess", "hello", "world"};

    CMD::Options options = CMD::Options(3, args);

    CHECK(options.getUserInput().size() == 2);
}