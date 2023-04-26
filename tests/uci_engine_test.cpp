#include "../src/engines/uci_engine.hpp"

#include <cassert>
#include <chrono>
#include <thread>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Uci Engine Communication Tests") {
    TEST_CASE("Testing the EngineProcess class") {
        UciEngine uci_engine;
#ifdef _WIN64
        uci_engine.startEngine("./tests/data/engine/dummy_engine.exe");
#else
        uci_engine.startEngine("./tests/data/engine/dummy_engine");
#endif

        uci_engine.sendUci();
        auto uci = uci_engine.readUci();
        auto uciOutput = uci_engine.output();

        CHECK(uci);
        CHECK(uciOutput.size() == 3);
        CHECK(uciOutput[0] == "line0");
        CHECK(uciOutput[1] == "line1");
        CHECK(uciOutput[2] == "uciok");
        CHECK(uci_engine.isResponsive());

        bool timeout = false;

        uci_engine.writeEngine("sleep");
        auto sleeper = uci_engine.readEngine("done", 100);
        CHECK(uci_engine.timeout() == true);

        uci_engine.writeEngine("sleep");
        auto sleeper2 = uci_engine.readEngine("done", 5000);
        CHECK(timeout == false);
        CHECK(sleeper2.size() == 1);
        CHECK(sleeper2[0] == "done");

        uci_engine.writeEngine("quit");
    }

    TEST_CASE("Testing the EngineProcess class with lower level class functions") {
        UciEngine uci_engine;
#ifdef _WIN64
        uci_engine.startEngine("./tests/data/engine/dummy_engine.exe");
#else
        uci_engine.startEngine("./tests/data/engine/dummy_engine");
#endif

        uci_engine.writeEngine("uci");
        auto uciOutput = uci_engine.readEngine("uciok");

        CHECK(uci_engine.timeout() == false);
        CHECK(uciOutput.size() == 3);
        CHECK(uciOutput[0] == "line0");
        CHECK(uciOutput[1] == "line1");
        CHECK(uciOutput[2] == "uciok");

        uci_engine.writeEngine("isready");
        auto readyok = uci_engine.readEngine("readyok");
        CHECK(uci_engine.timeout() == false);
        CHECK(readyok.size() == 1);
        CHECK(readyok[0] == "readyok");

        uci_engine.writeEngine("sleep");
        auto sleeper = uci_engine.readEngine("done", 100);
        CHECK(uci_engine.timeout() == true);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        uci_engine.writeEngine("sleep");
        auto sleeper2 = uci_engine.readEngine("done", 5000);
        CHECK(uci_engine.timeout() == false);
        CHECK(sleeper2.size() == 1);
        CHECK(sleeper2[0] == "done");

        uci_engine.writeEngine("quit");
    }
}