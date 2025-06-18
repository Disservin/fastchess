#include <cli/cli.hpp>
#include <cli/cli_args.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess;

TEST_SUITE("Option Parsing Tests") {
    TEST_CASE("Should throw tc and st not usable together") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "tc=10/9.64",
            "st=5",
            "name=Alexandria-EA649FED",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "tc=40/1:9.65+0.1",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; cannot use tc and st together!", std::runtime_error);
    }

    TEST_CASE("Should throw no timecontrol specified") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "name=Alexandria-EA649FED",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "name=Alexandria-27E42728",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; no TimeControl specified!", std::runtime_error);
    }

    TEST_CASE("Should throw error too much concurrency") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-concurrency",
            "20000",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args},
                             "Error: Concurrency exceeds number of CPUs. Use --force-concurrency to override.",
                             std::runtime_error);
    }

    TEST_CASE("Should throw too many games") {
        const auto args = cli::Args{
            "fastchess.exe", "-games", "3", "-rounds", "25000",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error: Exceeded -game limit! Must be less than 2",
                             std::runtime_error);
    }

    TEST_CASE("Should throw invalid sprt config") {
        const auto args = cli::Args{
            "fastchess.exe", "-sprt", "alpha=0.05", "beta=0.05", "elo0=5", "elo1=-1.5", "model=bayesian",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; SPRT: elo0 must be less than elo1!", std::runtime_error);
    }

    TEST_CASE("Should throw invalid sprt config 2") {
        const auto args = cli::Args{
            "fastchess.exe", "-sprt", "alpha=0.55", "beta=0.55", "elo0=4", "elo1=5", "model=bayesian",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; SPRT: sum of alpha and beta must be less than 1!",
                             std::runtime_error);
    }

    TEST_CASE("Should throw invalid sprt config 3") {
        const auto args = cli::Args{
            "fastchess.exe", "-sprt", "alpha=0.05", "beta=0.05", "elo0=4", "elo1=5", "model=dsadsa",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; SPRT: invalid SPRT model!", std::runtime_error);
    }

    TEST_CASE("Should throw invalid sprt config 4") {
        const auto args = cli::Args{
            "fastchess.exe", "-sprt", "alpha=1.05", "beta=0.05", "elo0=4", "elo1=5", "model=logistic",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; SPRT: alpha must be a decimal number between 0 and 1!",
                             std::runtime_error);
    }

    TEST_CASE("Should throw invalid sprt config 5") {
        const auto args = cli::Args{
            "fastchess.exe", "-sprt", "alpha=0.05", "beta=1.05", "elo0=4", "elo1=5", "model=logistic",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; SPRT: beta must be a decimal number between 0 and 1!",
                             std::runtime_error);
    }

    TEST_CASE("Should throw no chess960 opening book") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-variant",
            "fischerandom",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error: Please specify a Chess960 opening book",
                             std::runtime_error);
    }

    TEST_CASE("Should throw not enough engines") {
        const auto args = cli::Args{
            "fastchess.exe", "-engine", "dir=./", "cmd=app/tests/mock/engine/dummy_engine", "depth=5",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error: Need at least two engines to start!",
                             std::runtime_error);
    }

    TEST_CASE("Should throw no engine name") {
        const auto args = cli::Args{
            "fastchess.exe",    "-engine", "dir=./", "cmd=app/tests/mock/engine/dummy_engine",
            "depth=5",          "-engine", "dir=./", "cmd=app/tests/mock/engine/dummy_engine",
            "tc=40/1:9.65+0.1",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; please specify a name for each engine!",
                             std::runtime_error);
    }

    TEST_CASE("Should throw invalid tc") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "tc=10/0+0",
            "name=Alexandria-EA649FED",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "tc=10/0+0",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Error; no TimeControl specified!", std::runtime_error);
    }

    TEST_CASE("Should throw engine with same name") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "tc=10/1+0",
            "name=Alexandria-EA649FED",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "name=Alexandria-EA649FED",
            "tc=10/1+0",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args},
                             "Error: Engine with the same name are not allowed!: Alexandria-EA649FED",
                             std::runtime_error);
    }

    TEST_CASE("Should throw engine with invalid restart") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "tc=10/1+0",
            "restart=true",
            "name=Alexandria-EA649FED",
            "-engine",
            "dir=./",
            "cmd=app/tests/mock/engine/dummy_engine",
            "name=Alexandria-27E42728",
            "tc=10/1+0",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args},
                             "Error while reading option \"-engine\" with value \"name=Alexandria-EA649FED\"\nReason: "
                             "Invalid parameter (must be either \"on\" or \"off\"): true",
                             std::runtime_error);
    }

    TEST_CASE("Should throw engine not found") {
        const auto args = cli::Args{
            "fastchess.exe", "-engine",
            "dir=./",        "cmd=foo.exe",
            "tc=10/1+0",     "name=Alexandria-EA649FED",
            "-engine",       "dir=./",
            "cmd=foo.exe",   "name=Alexandria-EA649FED",
            "tc=10/1+0",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args}, "Engine not found at: ./foo.exe", std::runtime_error);
    }

    TEST_CASE("Should throw error empty TB paths") {
        const auto args = cli::Args{
            "fastchess.exe",
            "-tb",
        };

        CHECK_THROWS_WITH_AS(cli::OptionsParser{args},
                             "Error: Must provide a ;-separated list of Syzygy tablebase directories.",
                             std::runtime_error);
    }

    TEST_CASE("General Config Parsing") {
        const auto args = cli::Args{"fastchess.exe",
                                    "-engine",
                                    "dir=./",
                                    "cmd=app/tests/mock/engine/dummy_engine",
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
                                    "file=PGNs/Alexandria-EA649FED_vs_Alexandria-27E42728",
                                    "-use-affinity",
                                    "0-1"};

        cli::OptionsParser options = cli::OptionsParser(args);

        CHECK(options.getTournamentConfig().affinity_cpus.size() == 2);
        CHECK(options.getTournamentConfig().affinity_cpus[0] == 0);
        CHECK(options.getTournamentConfig().affinity_cpus[1] == 1);

        auto configs = options.getEngineConfigs();

        CHECK(configs.size() == 2);
        EngineConfiguration config0 = configs[0];
        EngineConfiguration config1 = configs[1];
        CHECK(config0.name == "Alexandria-EA649FED");
        CHECK(config0.limit.tc.moves == 0);
        CHECK(config0.limit.tc.time == 0);
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

    TEST_CASE("General Config Parsing 2") {
        const auto args = cli::Args{"fastchess.exe",
                                    "-engine",
                                    "dir=./",
                                    "cmd=app/tests/mock/engine/dummy_engine",
                                    "name=Alexandria-EA649FED",
                                    "tc=10/9.64",
                                    "-engine",
                                    "dir=./",
                                    "cmd=app/tests/mock/engine/dummy_engine",
                                    "name=Alexandria-27E42728",
                                    "tc=10/9.64",
                                    "-recover",
                                    "-concurrency",
                                    "2",
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
                                    "-report",
                                    "penta=false",
                                    "-use-affinity",
                                    "-srand",
                                    "1234",
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

        cli::OptionsParser options = cli::OptionsParser(args);
        auto gameOptions           = options.getTournamentConfig();

        // Test proper cli settings
        CHECK(gameOptions.recover);
        CHECK(gameOptions.concurrency == 2);
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
        CHECK(gameOptions.seed == 1234);
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
