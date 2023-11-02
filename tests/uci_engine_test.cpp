#include <types/engine_config.hpp>
#include <engines/uci_engine.hpp>

#include <chrono>
#include <thread>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Uci Engine Communication Tests") {
    TEST_CASE("Testing the EngineProcess class") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = "./tests/mock/engine/dummy_engine.exe";
#else
        config.cmd = "./tests/data/engine/dummy_engine";
#endif
        std::vector<int> cpus = {};

        UciEngine uci_engine = UciEngine(config, cpus);

        uci_engine.startEngine();

        uci_engine.sendUci();
        auto uci       = uci_engine.readUci();
        auto uciOutput = uci_engine.output();

        CHECK(uci);
        CHECK(uciOutput.size() == 3);
        CHECK(uciOutput[0] == "line0");
        CHECK(uciOutput[1] == "line1");
        CHECK(uciOutput[2] == "uciok");
        CHECK(uci_engine.isResponsive());

        bool timedout = false;

        uci_engine.writeEngine("sleep");
        uci_engine.readEngine("done", std::chrono::milliseconds(100));
        CHECK(uci_engine.timedout() == true);

        uci_engine.writeEngine("sleep");
        uci_engine.readEngine("done", std::chrono::milliseconds(5000));
        CHECK(timedout == false);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0] == "done");

        uci_engine.writeEngine("quit");
    }

    TEST_CASE("Testing the EngineProcess class with lower level class functions") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = "./tests/mock/engine/dummy_engine.exe";
#else
        config.cmd = "./tests/data/engine/dummy_engine";
#endif
        std::vector<int> cpus = {};

        UciEngine uci_engine = UciEngine(config, cpus);

        uci_engine.startEngine();

        uci_engine.writeEngine("uci");
        uci_engine.readEngine("uciok");

        CHECK(uci_engine.timedout() == false);
        CHECK(uci_engine.output().size() == 3);
        CHECK(uci_engine.output()[0] == "line0");
        CHECK(uci_engine.output()[1] == "line1");
        CHECK(uci_engine.output()[2] == "uciok");

        uci_engine.writeEngine("isready");
        uci_engine.readEngine("readyok");
        CHECK(uci_engine.timedout() == false);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0] == "readyok");

        uci_engine.writeEngine("sleep");
        uci_engine.readEngine("done", std::chrono::milliseconds(100));
        CHECK(uci_engine.timedout() == true);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        uci_engine.writeEngine("sleep");
        uci_engine.readEngine("done", std::chrono::milliseconds(5000));
        CHECK(uci_engine.timedout() == false);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0] == "done");

        uci_engine.writeEngine("quit");
    }
}