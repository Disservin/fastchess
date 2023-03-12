#include "doctest/doctest.hpp"

#include "../src/engines/uci_engine.hpp"

#include <cassert>

using namespace fast_chess;

TEST_CASE("Testing the EngineProcess class")
{
    UciEngine uci_engine;
#ifdef _WIN64
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
    CHECK(uci_engine.isResponsive());

    bool timeout = false;

    uci_engine.writeProcess("sleep");
    auto sleeper = uci_engine.readProcess("done", timeout, 100);
    CHECK(timeout == true);

    uci_engine.writeProcess("sleep");
    auto sleeper2 = uci_engine.readProcess("done", timeout, 5000);
    CHECK(timeout == false);
    CHECK(sleeper2.size() == 1);
    CHECK(sleeper2[0] == "done");

    uci_engine.writeProcess("quit");
}

TEST_CASE("Testing the EngineProcess class with lower level class functions")
{
    UciEngine uci_engine;
#ifdef _WIN64
    uci_engine.startEngine("./data/engine/dummy_engine.exe");
#else
    uci_engine.startEngine("./data/engine/dummy_engine");
#endif

    bool timeout = false;
    uci_engine.writeProcess("uci");
    auto uciOutput = uci_engine.readProcess("uciok", timeout);

    CHECK(timeout == false);
    CHECK(uciOutput.size() == 3);
    CHECK(uciOutput[0] == "line0");
    CHECK(uciOutput[1] == "line1");
    CHECK(uciOutput[2] == "uciok");

    uci_engine.writeProcess("isready");
    auto readyok = uci_engine.readProcess("readyok", timeout);
    CHECK(timeout == false);
    CHECK(readyok.size() == 1);
    CHECK(readyok[0] == "readyok");

    uci_engine.writeProcess("sleep");
    auto sleeper = uci_engine.readProcess("done", timeout, 100);
    CHECK(timeout == true);

    uci_engine.writeProcess("sleep");
    auto sleeper2 = uci_engine.readProcess("done", timeout, 5000);
    CHECK(timeout == false);
    CHECK(sleeper2.size() == 1);
    CHECK(sleeper2[0] == "done");

    uci_engine.writeProcess("quit");
}