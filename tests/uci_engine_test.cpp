#include <engines/uci_engine.hpp>
#include <types/engine_config.hpp>

#include <chrono>
#include <string_view>
#include <thread>

#include "doctest/doctest.hpp"

using namespace fast_chess;

const std::string path = "./tests/mock/engine/";

TEST_SUITE("Uci Engine Communication Tests") {
    TEST_CASE("Testing the EngineProcess class") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = path + "dummy_engine.exe";
#else
        config.cmd = path + "dummy_engine";
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

        uci_engine.writeEngine("sleep");
        const auto res = uci_engine.readEngine("done", std::chrono::milliseconds(100));
        CHECK(res == Process::Status::TIMEOUT);

        uci_engine.writeEngine("sleep");
        const auto res2 = uci_engine.readEngine("done", std::chrono::milliseconds(5000));
        CHECK(res2 == Process::Status::OK);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0] == "done");

        uci_engine.writeEngine("quit");
    }

    TEST_CASE("Testing the EngineProcess class with lower level class functions") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = path + "dummy_engine.exe";
#else
        config.cmd = path + "dummy_engine";
#endif
        std::vector<int> cpus = {};

        UciEngine uci_engine = UciEngine(config, cpus);

        uci_engine.startEngine();

        uci_engine.writeEngine("uci");
        const auto res = uci_engine.readEngine("uciok");

        CHECK(res == Process::Status::OK);
        CHECK(uci_engine.output().size() == 3);
        CHECK(uci_engine.output()[0] == "line0");
        CHECK(uci_engine.output()[1] == "line1");
        CHECK(uci_engine.output()[2] == "uciok");

        uci_engine.writeEngine("isready");
        const auto res2 = uci_engine.readEngine("readyok");
        CHECK(res2 == Process::Status::OK);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0] == "readyok");

        uci_engine.writeEngine("sleep");
        const auto res3 = uci_engine.readEngine("done", std::chrono::milliseconds(100));
        CHECK(res3 == Process::Status::TIMEOUT);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        uci_engine.writeEngine("sleep");
        const auto res4 = uci_engine.readEngine("done", std::chrono::milliseconds(5000));
        CHECK(res4 == Process::Status::OK);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0] == "done");

        uci_engine.writeEngine("quit");
    }
}