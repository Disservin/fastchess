#include "../src/options.h"
#include "doctest/doctest.h"

#include <cassert>

TEST_CASE("Testing Engine options parsing")
{
    const char *argv[] = {"fast-chess.exe",
                          "-engine",
                          "dir=Engines/",
                          "cmd=./Alexandria-EA649FED.exe",
                          "proto=uci",
                          "tc=10/9.64",
                          "option.Threads=1",
                          "option.Hash=16",
                          "name=Alexandria-EA649FED",
                          "-engine",
                          "dir=Engines/",
                          "cmd=./Alexandria-27E42728.exe",
                          "proto=uci",
                          "tc=9.64+0.10",
                          "option.Threads=1",
                          "option.Hash=32",
                          "name=Alexandria-27E42728",
                          "-openings",
                          "file=Books/Pohl.epd",
                          "format=epd",
                          "order=random",
                          "plies=16",
                          "-pgnout",
                          "PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};
    CMD::Options options = CMD::Options(22, argv);
    CHECK(options.configs.size() == 2);
    CHECK(options.configs[0].name == "Alexandria-EA649FED");
    CHECK(options.configs[0].tc.moves == 10);
    CHECK(options.configs[0].tc.time == 9640);
    CHECK(options.configs[0].tc.increment == 0);
    CHECK(options.configs[0].nodes == 0);
    CHECK(options.configs[0].plies == 0);
    CHECK(options.configs[0].options.at(0).first == "option.Threads");
    CHECK(options.configs[0].options.at(0).second == "1");
    CHECK(options.configs[0].options.at(1).first == "option.Hash");
    CHECK(options.configs[0].options.at(1).second == "16");
    CHECK(options.configs[1].name == "Alexandria-27E42728");
    CHECK(options.configs[1].tc.moves == 0);
    CHECK(options.configs[1].tc.time == 9640);
    CHECK(options.configs[1].tc.increment == 100);
    CHECK(options.configs[1].nodes == 0);
    CHECK(options.configs[1].plies == 0);
    CHECK(options.configs[1].options.at(0).first == "option.Threads");
    CHECK(options.configs[1].options.at(0).second == "1");
    CHECK(options.configs[1].options.at(1).first == "option.Hash");
    CHECK(options.configs[1].options.at(1).second == "32");
}