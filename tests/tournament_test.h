#pragma once

#include "../src/chess/helper.h"
#include "../src/options.h"
#include "../src/tournament.h"

TEST_CASE("Test Tournament")
{
    CMD::GameManagerOptions mang = CMD::GameManagerOptions();
    mang.games = 1;
    mang.rounds = 2;
    EngineConfiguration engine;
    EngineConfiguration engine_2;

    engine.plies = 1;
    engine_2.plies = 1;

    engine.name = "engine";
    engine_2.name = "engine_2";

#ifdef _WIN64
    engine.cmd = "./data/engine/dummy_engine.exe";
    engine_2.cmd = "./data/engine/dummy_engine.exe";
#else
    engine.cmd = "./data/engine/dummy_engine";
    engine_2.cmd = "./data/engine/dummy_engine";
#endif

    Tournament tour(false);
    tour.setStorePGN(true);
    tour.loadConfig(mang);

    tour.startTournament({engine, engine_2});

    std::string pgn =
        "[Event \"\"]\n[Site \"?\"]\n[Date \"\"]\n[Round \"1\"]\n[White \"engine\"]\n[Black \"engine_2\"]\n[Result "
        "\"0-1\"]\n[FEN "
        "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n[GameDuration \"\"]\n[GameEndTime "
        "\"\"]\n[GameStartTime \"\"]\n[PlyCount \"4\"]\n[TimeControl \"0\"]\n\n1. f3 {0.00/0 0ms} e5 {0.00/0 0ms} 2. "
        "g4 {0.00/0 0ms} Qh4# {0.00/0 0ms}  0-1\n";

    CHECK(tour.getPGNS()[0] == pgn);
}