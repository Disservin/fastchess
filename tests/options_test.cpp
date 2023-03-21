#include "doctest/doctest.hpp"

#include "options.hpp"

#include <cassert>

using namespace fast_chess;

TEST_SUITE("Option Parsing Tests")
{

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
                              "tc=40/9.65",
                              "option.Threads=1",
                              "option.Hash=32",
                              "name=Alexandria-27E42728",
                              "-openings",
                              "file=Books/Pohl.epd",
                              "format=epd",
                              "order=random",
                              "plies=16",
                              "-rounds",
                              "50",
                              "-games",
                              "2",
                              "-pgnout",
                              "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};
        CMD::Options options = CMD::Options(26, argv);

        auto configs = options.getEngineConfigs();

        CHECK(configs.size() == 2);
        EngineConfiguration config0 = configs[0];
        EngineConfiguration config1 = configs[1];
        CHECK(config0.name == "Alexandria-EA649FED");
        CHECK(config0.tc.moves == 10);
        CHECK(config0.tc.time == 9640);
        CHECK(config0.tc.increment == 0);
        CHECK(config0.nodes == 0);
        CHECK(config0.plies == 0);
        CHECK(config0.options.at(0).first == "Threads");
        CHECK(config0.options.at(0).second == "1");
        CHECK(config0.options.at(1).first == "Hash");
        CHECK(config0.options.at(1).second == "16");
        CHECK(config1.name == "Alexandria-27E42728");
        CHECK(config1.tc.moves == 40);
        CHECK(config1.tc.time == 9650);
        CHECK(config1.tc.increment == 0);
        CHECK(config1.nodes == 0);
        CHECK(config1.plies == 0);
        CHECK(config1.options.at(0).first == "Threads");
        CHECK(config1.options.at(0).second == "1");
        CHECK(config1.options.at(1).first == "Hash");
        CHECK(config1.options.at(1).second == "32");
    }

    TEST_CASE("Testing Cli Options Parsing")
    {
        const char *argv[] = {"fast-chess.exe",
                              "-repeat",
                              "-recover",
                              "-concurrency",
                              "8",
                              "-rounds",
                              "256",
                              "-games",
                              "2",
                              "-sprt",
                              "alpha=0.5",
                              "beta=0.5",
                              "elo0=0",
                              "elo1=5",
                              "-openings",
                              "file=Books/Pohl.epd",
                              "format=epd",
                              "order=random",
                              "plies=16",
                              "-pgnout",
                              "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};
        CMD::Options options = CMD::Options(21, argv);
        auto gameOptions = options.getGameOptions();

        // Test proper cli settings
        CHECK(gameOptions.recover == true);
        CHECK(gameOptions.concurrency == 8);
        CHECK(gameOptions.games == 2);
        CHECK(gameOptions.rounds == 256);
        // Test Sprt options parsing
        CHECK(gameOptions.sprt.alpha == 0.5);
        CHECK(gameOptions.sprt.beta == 0.5);
        CHECK(gameOptions.sprt.elo0 == 0);
        CHECK(gameOptions.sprt.elo1 == 5.0);
        CHECK(gameOptions.pgn.file == "PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728");
        // Test opening settings parsing
        CHECK(gameOptions.opening.file == "Books/Pohl.epd");
        CHECK(gameOptions.opening.format == "epd");
        CHECK(gameOptions.opening.order == "random");
        CHECK(gameOptions.opening.plies == 16);
    }
}