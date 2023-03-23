#include "matchmaking/tournament.hpp"

#include "chess/helper.hpp"
#include "doctest/doctest.hpp"
#include "options.hpp"

using namespace fast_chess;

TEST_SUITE("Tournament Tests") {
    TEST_CASE("Test Tournament") {
#ifdef _WIN64
        auto path = "cmd=./tests/data/engine/dummy_engine.exe";
#else
        auto path = "cmd=./tests/data/engine/dummy_engine";
#endif

        const char *argv[] = {"./fast-chess",
                              "-engine",
                              "name=engine",
                              path,
                              "-engine",
                              "name=engine_2",
                              path,
                              "-each",
                              "plies=1",
                              "-rounds",
                              "1",
                              "-games",
                              "1",
                              "-concurrency",
                              "1"};

        CMD::Options options = CMD::Options(15, argv);

        Tournament tour(options.getGameOptions());

        tour.startTournament(options.getEngineConfigs());

        std::string pgn =
            "[Event \"?\"]\n[Site \"?\"]\n[Date \"\"]\n[Round \"1\"]\n[White "
            "\"engine\"]\n[Black \"engine_2\"]\n[Result "
            "\"0-1\"]\n[FEN "
            "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 "
            "1\"]\n[GameDuration \"\"]\n[GameEndTime "
            "\"\"]\n[GameStartTime \"\"]\n[PlyCount \"4\"]\n[TimeControl \"0\"]\n\n1. f3 "
            "{0.00/0 0.000s} e5 {0.00/0 0.000s} 2. "
            "g4 {0.00/0 0.000s} Qh4# {0.00/0 0.000s} 0-1\n";

#ifdef TESTS
        CHECK(tour.getPgns()[0] == pgn);
#endif  // TESTS
    }
}