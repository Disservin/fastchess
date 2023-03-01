#pragma once

#include "../src/engines/engine.h"

#include <cassert>

TEST_CASE("Testing the EngineProcess class")
{
#ifdef _WIN64
    Engine engine("./data/engine/dummy_engine.exe");
#else
    Engine engine("./data/engine/dummy_engine");
#endif

    bool timeout = false;

    engine.writeProcess("uci");
    auto uciOutput = engine.readProcess("uciok", timeout);

    CHECK(timeout == false);
    CHECK(uciOutput.size() == 3);
    CHECK(uciOutput[0] == "line0");
    CHECK(uciOutput[1] == "line1");
    CHECK(uciOutput[2] == "uciok");

    engine.writeProcess("isready");
    auto readyok = engine.readProcess("readyok", timeout);
    CHECK(timeout == false);
    CHECK(readyok.size() == 1);
    CHECK(readyok[0] == "readyok");

    engine.writeProcess("sleep");
    auto sleeper = engine.readProcess("done", timeout, 100);
    CHECK(timeout == true);

    engine.writeProcess("sleep");
    auto sleeper2 = engine.readProcess("done", timeout, 5000);
    CHECK(timeout == false);
    CHECK(sleeper2.size() == 1);
    CHECK(sleeper2[0] == "done");

    engine.writeProcess("quit");
}

TEST_CASE("Testing the EngineProcess class with setCmd")
{
#ifdef _WIN64
    Engine engine;
    engine.initProcess("./data/engine/dummy_engine.exe");
#else
    Engine engine;
    engine.initProcess("./data/engine/dummy_engine");

#endif
    bool timeout = false;
    engine.writeProcess("uci");
    auto uciOutput = engine.readProcess("uciok", timeout);

    CHECK(timeout == false);
    CHECK(uciOutput.size() == 3);
    CHECK(uciOutput[0] == "line0");
    CHECK(uciOutput[1] == "line1");
    CHECK(uciOutput[2] == "uciok");

    engine.writeProcess("isready");
    auto readyok = engine.readProcess("readyok", timeout);
    CHECK(timeout == false);
    CHECK(readyok.size() == 1);
    CHECK(readyok[0] == "readyok");

    engine.writeProcess("sleep");
    auto sleeper = engine.readProcess("done", timeout, 100);
    CHECK(timeout == true);

    engine.writeProcess("sleep");
    auto sleeper2 = engine.readProcess("done", timeout, 5000);
    CHECK(timeout == false);
    CHECK(sleeper2.size() == 1);
    CHECK(sleeper2[0] == "done");

    engine.writeProcess("quit");
}