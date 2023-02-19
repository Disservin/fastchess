#include "../src/engine.h"
#include "doctest/doctest.h"

#include <cassert>

TEST_CASE("Testing the EngineProcess class")
{
#ifdef _WIN32
    Engine engine("./data/engine/dummy_engine.exe");
#else
    Engine engine("./data/engine/dummy_engine");
#endif

    bool timedOut = false;

    engine.writeProcess("uci");
    auto uciOutput = engine.readProcess("uciok", timedOut);

    CHECK(timedOut == false);
    CHECK(uciOutput.size() == 3);
    CHECK(uciOutput[0] == "line0");
    CHECK(uciOutput[1] == "line1");
    CHECK(uciOutput[2] == "uciok");

    engine.writeProcess("isready");
    auto readyok = engine.readProcess("readyok", timedOut);
    CHECK(timedOut == false);
    CHECK(readyok.size() == 1);
    CHECK(readyok[0] == "readyok");

    engine.writeProcess("sleep");
    auto sleeper = engine.readProcess("done", timedOut, 100);
    CHECK(timedOut == true);

    engine.writeProcess("sleep");
    auto sleeper2 = engine.readProcess("done", timedOut, 5000);
    CHECK(timedOut == false);
    CHECK(sleeper2.size() == 1);
    CHECK(sleeper2[0] == "done");
}