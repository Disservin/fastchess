#include "doctest/doctest.hpp"

#include "../src/chess/helper.hpp"
#include "../src/options.hpp"
#include "../src/tournament.hpp"

using namespace fast_chess;

TEST_CASE("Test Tournament")
{
#ifdef _WIN64
    auto path = "cmd=./data/engine/dummy_engine.exe";
#else
    auto path = "cmd=./data/engine/dummy_engine";
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

    tour.setStorePGN(true);
    tour.startTournament(options.getEngineConfigs());

    std::string pgn = "[Event \"?\"]\n[Site \"?\"]\n[Date \"\"]\n[Round \"1\"]\n[White "
                      "\"engine\"]\n[Black \"engine_2\"]\n[Result "
                      "\"0-1\"]\n[FEN "
                      "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 "
                      "1\"]\n[GameDuration \"\"]\n[GameEndTime "
                      "\"\"]\n[GameStartTime \"\"]\n[PlyCount \"4\"]\n[TimeControl \"0\"]\n\n1. f3 "
                      "{0.00/0 } e5 {0.00/0 } 2. "
                      "g4 {0.00/0 } Qh4# {0.00/0 } 0-1\n";

    CHECK(tour.getPGNS()[0] == pgn);
}