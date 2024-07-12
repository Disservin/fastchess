#include <cli/cli.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Option Parsing Tests") {
    TEST_CASE("Testing Engine options parsing") {
        const char *argv[] = {"fast-chess.exe",
                              "-engine",
                              "dir=./",
                              "cmd=app/tests/mock/engine/dummy_engine",
                              "tc=10/9.64",
                              "depth=5",
                              "st=5",
                              "nodes=5000",
                              "option.Threads=1",
                              "option.Hash=16",
                              "name=Alexandria-EA649FED",
                              "-engine",
                              "dir=./",
                              "cmd=app/tests/mock/engine/dummy_engine",
                              "tc=40/1:9.65+0.1",
                              "timemargin=243",
                              "plies=7",
                              "option.Threads=1",
                              "option.Hash=32",
                              "name=Alexandria-27E42728",
                              "-openings",
                              "file=./app/tests/data/test.epd",
                              "format=epd",
                              "order=random",
                              "plies=16",
                              "-rounds",
                              "50",
                              "-games",
                              "2",
                              "-pgnout",
                              "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728"};

        cli::OptionsParser options = cli::OptionsParser(31, argv);

        auto configs = options.getEngineConfigs();

        CHECK(configs.size() == 2);
        EngineConfiguration config0 = configs[0];
        EngineConfiguration config1 = configs[1];
        CHECK(config0.name == "Alexandria-EA649FED");
        CHECK(config0.limit.tc.moves == 10);
        CHECK(config0.limit.tc.time == 9640);
        CHECK(config0.limit.tc.increment == 0);
        CHECK(config0.limit.tc.timemargin == 0);
        CHECK(config0.limit.tc.fixed_time == 5000);
        CHECK(config0.limit.nodes == 5000);
        CHECK(config0.limit.plies == 5);
        CHECK(config0.options.at(0).first == "Threads");
        CHECK(config0.options.at(0).second == "1");
        CHECK(config0.options.at(1).first == "Hash");
        CHECK(config0.options.at(1).second == "16");
        CHECK(config1.name == "Alexandria-27E42728");
        CHECK(config1.limit.tc.moves == 40);
        CHECK(config1.limit.tc.time == 69650);
        CHECK(config1.limit.tc.fixed_time == 0);
        CHECK(config1.limit.tc.increment == 100);
        CHECK(config1.limit.tc.timemargin == 243);
        CHECK(config1.limit.nodes == 0);
        CHECK(config1.limit.plies == 7);
        CHECK(config1.options.at(0).first == "Threads");
        CHECK(config1.options.at(0).second == "1");
        CHECK(config1.options.at(1).first == "Hash");
        CHECK(config1.options.at(1).second == "32");
    }

    TEST_CASE("Testing Cli Options Parsing") {
        const char *argv[]         = {"fast-chess.exe",
                                      "-recover",
                                      "-concurrency",
                                      "8",
                                      "-ratinginterval",
                                      "2",
                                      "-scoreinterval",
                                      "3",
                                      "-autosaveinterval",
                                      "4",
                                      "-rounds",
                                      "256",
                                      "-draw",
                                      "movenumber=40",
                                      "movecount=3",
                                      "score=15",
                                      "-resign",
                                      "movecount=5",
                                      "score=600",
                                      "twosided=true",
                                      "-maxmoves",
                                      "150",
                                      "-games",
                                      "1",
                                      "-sprt",
                                      "alpha=0.05",
                                      "beta=0.05",
                                      "elo0=-1.5",
                                      "elo1=5",
                                      "model=bayesian",
                                      "-openings",
                                      "file=./app/tests/data/test.epd",
                                      "format=epd",
                                      "order=sequential",
                                      "plies=16",
                                      "start=4",
                                      "-variant",
                                      "fischerandom",
                                      "-output",
                                      "format=cutechess",
                                      "-srand",
                                      "1234",
                                      "-randomseed",
                                      "-report",
                                      "penta=false",
                                      "-use-affinity",
                                      "-epdout",
                                      "file=EPDs/Alexandria-EA649FED_vs_Alexandria-27E42728",
                                      "-pgnout",
                                      "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728",
                                      "nodes=true",
                                      "nps=true",
                                      "seldepth=true",
                                      "hashfull=true",
                                      "tbhits=true",
                                      "min=true"};
        cli::OptionsParser options = cli::OptionsParser(56, argv);
        auto gameOptions           = options.getTournamentConfig();

        // Test proper cli settings
        CHECK(gameOptions.recover);
        CHECK(gameOptions.concurrency == 8);
        CHECK(gameOptions.ratinginterval == 2);
        CHECK(gameOptions.scoreinterval == 3);
        CHECK(gameOptions.autosaveinterval == 4);
        CHECK(gameOptions.games == 1);
        CHECK(gameOptions.rounds == 256);
        CHECK(gameOptions.variant == VariantType::FRC);
        CHECK(gameOptions.draw.move_number == 40);
        CHECK(gameOptions.draw.move_count == 3);
        CHECK(gameOptions.draw.score == 15);
        CHECK(gameOptions.draw.enabled);
        CHECK(gameOptions.resign.move_count == 5);
        CHECK(gameOptions.resign.score == 600);
        CHECK(gameOptions.resign.twosided);
        CHECK(gameOptions.resign.enabled);
        CHECK(gameOptions.maxmoves.move_count == 150);
        CHECK(gameOptions.maxmoves.enabled);
        CHECK(gameOptions.output == OutputType::CUTECHESS);
        CHECK(gameOptions.affinity);
        CHECK(gameOptions.report_penta == false);
        // Test Sprt options parsing
        CHECK(gameOptions.sprt.enabled);
        CHECK(gameOptions.sprt.alpha == 0.05);
        CHECK(gameOptions.sprt.beta == 0.05);
        CHECK(gameOptions.sprt.elo0 == -1.5);
        CHECK(gameOptions.sprt.elo1 == 5.0);
        CHECK(gameOptions.sprt.model == "bayesian");
        CHECK(gameOptions.pgn.file == "PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728");
        CHECK(gameOptions.pgn.track_nodes);
        CHECK(gameOptions.pgn.track_seldepth);
        CHECK(gameOptions.pgn.track_nps);
        CHECK(gameOptions.pgn.track_hashfull);
        CHECK(gameOptions.pgn.track_tbhits);
        CHECK(gameOptions.pgn.min);
        CHECK(gameOptions.epd.file == "EPDs/Alexandria-EA649FED_vs_Alexandria-27E42728");
        // Test opening settings parsing
        CHECK(gameOptions.opening.file == "./app/tests/data/test.epd");
        CHECK(gameOptions.opening.format == FormatType::EPD);
        CHECK(gameOptions.opening.order == OrderType::SEQUENTIAL);
        CHECK(gameOptions.opening.plies == 16);
        CHECK(gameOptions.opening.start == 4);
    }
}
