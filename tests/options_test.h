#include "../src/options.h"
#include "doctest/doctest.h"

#include <cassert>

TEST_CASE("Testing Engine options parsing")
{
    const char *argv[] = {"fast-chess.exe",
                          "-engine",
                          "dir=Engines/",
                          "cmd=./Alexandria-EA649FED.exe",
                          "tc=10/9.64",
                          "option.Threads=1",
                          "option.Hash=16",
                          "name=Alexandria-EA649FED",
                          "-engine",
                          "dir=Engines/",
                          "cmd=./Alexandria-27E42728.exe",
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
TEST_CASE("Testing Cli options parsing")
{

    const char *argv[] = {"fast-chess.exe",
                          "-repeat",
                          "-recover",
                          "-concurrency",
                          "8",
                          "-games",
                          "256",
                          "-openings",
                          "file=Books/Pohl.epd",
                          "format=epd",
                          "order=random",
                          "plies=16",
                          "-pgnout",
                          "PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};
    CMD::Options options = CMD::Options(14, argv);
    // Test proper cli settings
    CHECK(options.game_options.repeat == true);
    CHECK(options.game_options.recover == true);
    CHECK(options.game_options.concurrency == 8);
    // CHECK(options.game_options.games == 256);
    //  Test opening settings parsing
    // CHECK(options.game_options.opening_options.file == "Books/Pohl.epd");
    // CHECK(options.game_options.opening_options.format == "epd");
    // CHECK(options.game_options.opening_options.order == "random");
    // CHECK(options.game_options.opening_options.plies == 16);
    //  Test pgn settings parsing
    // CHECK(options.game_options.pgn_options.file == "PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728");
}