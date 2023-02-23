#pragma once

#include "../src/uci_engine.h"

#include <cassert>

TEST_CASE("Testing the EngineProcess class")
{
    UciEngine uci_engine;
#ifdef _WIN32
    uci_engine.startEngine("./data/engine/dummy_engine.exe");
#else
    uci_engine.startEngine("./data/engine/dummy_engine");
#endif

    uci_engine.sendUci();
    auto uciOutput = uci_engine.readUci();

    CHECK(uciOutput.size() == 3);
    CHECK(uciOutput[0] == "line0");
    CHECK(uciOutput[1] == "line1");
    CHECK(uciOutput[2] == "uciok");
}
