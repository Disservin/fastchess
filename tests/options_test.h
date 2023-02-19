#include "../src/options.h"
#include "doctest/doctest.h"

#include <cassert>

TEST_CASE("Testing options")
{
    const char *argv[] = {"fast-chess.exe",
                          "-engine dir=Engines/",
                          "cmd=./Alexandria-EA649FED.exe",
                          "proto=uci"
                          "tc=9.64+0.10",
                          "option.Threads=1",
                          "option.Hash=16",
                          "name=Alexandria-EA649FED",
                          "-engine",
                          "dir=Engines/",
                          "cmd=./Alexandria-27E42728.exe",
                          "proto=uci",
                          "tc=9.64+0.10",
                          "option.Threads=1",
                          "option.Hash=16",
                          "name=Alexandria-27E42728",
                          "-openings",
                          "file=Books/Pohl.epd",
                          "format=epd",
                          "order=random",
                          "plies=16",
                          "-pgnout",
                          "PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};
    CMD::Options options = CMD::Options(22, argv);

    CHECK(options.configs[0].name == "Alexandria-27E42728");
    CHECK(options.configs[0].tc.moves == 0);
    CHECK(options.configs[0].tc.time == 9640);
    CHECK(options.configs[0].tc.increment == 100);
    CHECK(options.configs[0].nodes == 0);
    CHECK(options.configs[0].plies == 0);
}