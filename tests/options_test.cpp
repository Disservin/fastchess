#include <cli/cli.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Option Parsing Tests") {
    TEST_CASE("Testing Engine options parsing") {
        const char *argv[] = {"fast-chess.exe",
                              "-engine",
                              "dir=./",
                              "cmd=tests/mock/engine/dummy_engine",
                              "tc=10/9.64",
                              "option.Threads=1",
                              "option.Hash=16",
                              "name=Alexandria-EA649FED",
                              "-engine",
                              "dir=./",
                              "cmd=tests/mock/engine/dummy_engine",
                              "tc=40/9.65",
                              "option.Threads=1",
                              "option.Hash=32",
                              "name=Alexandria-27E42728",
                              "-openings",
                              "file=./tests/data/test.epd",
                              "format=epd",
                              "order=random",
                              "plies=16",
                              "-rounds",
                              "50",
                              "-games",
                              "2",
                              "-pgnout",
                              "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};

        cli::OptionsParser options = cli::OptionsParser(26, argv);

        auto configs = options.getEngineConfigs();

        CHECK(configs.size() == 2);
        EngineConfiguration config0 = configs[0];
        EngineConfiguration config1 = configs[1];
        CHECK(config0.name == "Alexandria-EA649FED");
        CHECK(config0.limit.tc.moves == 10);
        CHECK(config0.limit.tc.time == 9640);
        CHECK(config0.limit.tc.increment == 0);
        CHECK(config0.limit.nodes == 0);
        CHECK(config0.limit.plies == 0);
        CHECK(config0.options.at(0).first == "Threads");
        CHECK(config0.options.at(0).second == "1");
        CHECK(config0.options.at(1).first == "Hash");
        CHECK(config0.options.at(1).second == "16");
        CHECK(config1.name == "Alexandria-27E42728");
        CHECK(config1.limit.tc.moves == 40);
        CHECK(config1.limit.tc.time == 9650);
        CHECK(config1.limit.tc.increment == 0);
        CHECK(config1.limit.nodes == 0);
        CHECK(config1.limit.plies == 0);
        CHECK(config1.options.at(0).first == "Threads");
        CHECK(config1.options.at(0).second == "1");
        CHECK(config1.options.at(1).first == "Hash");
        CHECK(config1.options.at(1).second == "32");
    }

    TEST_CASE("Testing Cli Options Parsing") {
        const char *argv[]         = {"fast-chess.exe",
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
                                      "file=./tests/data/test.epd",
                                      "format=epd",
                                      "order=random",
                                      "plies=16",
                                      "-pgnout",
                                      "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};
        cli::OptionsParser options = cli::OptionsParser(21, argv);
        auto gameOptions           = options.getGameOptions();

        // Test proper cli settings
        CHECK(gameOptions.recover);
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
        CHECK(gameOptions.opening.file == "./tests/data/test.epd");
        CHECK(gameOptions.opening.format == FormatType::EPD);
        CHECK(gameOptions.opening.order == OrderType::RANDOM);
        CHECK(gameOptions.opening.plies == 16);
    }
}
