#pragma once

#include "doctest/doctest.h"

#include "../src/helper.h"
#include "../src/options.h"
#include "../src/tournament.h"

TEST_CASE("Test Tournament")
{
    CMD::GameManagerOptions options;
    EngineConfiguration engine;

    engine.plies = 1;

#ifdef _WIN32
    engine.cmd = "./data/engine/dummy_engine.exe";
#else
    engine.cmd = "./data/engine/dummy_engine";
#endif

    Tournament tour(false);

    tour.startTournament({engine, engine});

    std::string pgn =
        "[Event \"\"]\n[Site \"?\"]\n[Date \"\"]\n[Round \"0\"]\n[White \"\"]\n[Black \"\"]\n[Result \"0-1\"]\n[FEN "
        "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n[GameDuration \"\"]\n[GameEndTime "
        "\"\"]\n[GameStartTime \"\"]\n[PlyCount \"4\"]\n[TimeControl \"0\"]\n\n1. f3 e5 2. g4 Qh4#  0-1\n";

    CHECK(tour.getPGNS()[0] == pgn);
}