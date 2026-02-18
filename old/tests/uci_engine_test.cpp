#include <core/config/config.hpp>
#include <engine/uci_engine.hpp>
#include <types/engine_config.hpp>

#include <chrono>
#include <string_view>
#include <thread>

#include <doctest/doctest.hpp>

using namespace fastchess;

namespace {

#ifdef _WIN64
const std::string_view path = "./app/tests/mock/engine/dummy_engine.exe";
#else
const std::string_view path = "./app/tests/mock/engine/dummy_engine";
#endif

class MockUciEngine : public engine::UciEngine {
   public:
    explicit MockUciEngine(const EngineConfiguration& config, bool realtime_logging)
        : engine::UciEngine(config, realtime_logging) {}
};

}  // namespace

TEST_SUITE("Uci Engine Communication Tests") {
    TEST_CASE("Test engine::UciEngine Args Simple") {
        EngineConfiguration config;
        config.cmd  = path;
        config.args = "arg1 arg2 arg3";

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        CHECK(uci_engine.start(/*cpus*/ std::nullopt));

        for (const auto& line : uci_engine.output()) {
            std::cout << line.line << std::endl;
        }

        CHECK(uci_engine.output().size() == 12);
        CHECK(uci_engine.output()[0].line == "argv[1]: arg1");
        CHECK(uci_engine.output()[1].line == "argv[2]: arg2");
        CHECK(uci_engine.output()[2].line == "argv[3]: arg3");

        CHECK(uci_engine.idName().has_value());
        CHECK(uci_engine.idName().value() == "Dummy Engine");

        CHECK(uci_engine.idAuthor().has_value());
        CHECK(uci_engine.idAuthor().value() == "Fastchess");
    }

    TEST_CASE("Test engine::UciEngine Args Complex") {
        EngineConfiguration config;
        config.cmd = path;
        config.args =
            "--backend=multiplexing "
            "--backend-opts=\"backend=cuda-fp16,(gpu=0),(gpu=1),(gpu=2),(gpu=3)\" "
            "--weights=lc0/BT4-1024x15x32h-swa-6147500.pb.gz --minibatch-size=132 "
            "--nncache=50000000 --threads=5";

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        CHECK(uci_engine.start(/*cpus*/ std::nullopt));

        for (const auto& line : uci_engine.output()) {
            std::cout << line.line << std::endl;
        }

        CHECK(uci_engine.output().size() == 15);
        CHECK(uci_engine.output()[0].line == "argv[1]: --backend=multiplexing");
        CHECK(uci_engine.output()[1].line ==
              "argv[2]: --backend-opts=backend=cuda-fp16,(gpu=0),(gpu=1),(gpu=2),(gpu=3)");
        CHECK(uci_engine.output()[2].line == "argv[3]: --weights=lc0/BT4-1024x15x32h-swa-6147500.pb.gz");
        CHECK(uci_engine.output()[3].line == "argv[4]: --minibatch-size=132");
        CHECK(uci_engine.output()[4].line == "argv[5]: --nncache=50000000");
        CHECK(uci_engine.output()[5].line == "argv[6]: --threads=5");
    }

    TEST_CASE("Testing the EngineProcess class") {
        EngineConfiguration config;
        config.cmd  = path;
        config.args = "arg1 arg2 arg3";

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        CHECK(uci_engine.start(/*cpus*/ std::nullopt));

        CHECK(uci_engine.output().size() == 12);
        CHECK(uci_engine.output()[0].line == "argv[1]: arg1");
        CHECK(uci_engine.output()[1].line == "argv[2]: arg2");
        CHECK(uci_engine.output()[2].line == "argv[3]: arg3");

        auto uciSuccess = uci_engine.uci();
        CHECK(uciSuccess);

        auto uci       = uci_engine.uciok();
        auto uciOutput = uci_engine.output();

        CHECK(uci);
        CHECK(uciOutput.size() == 9);
        CHECK(uciOutput[0].line == "id name Dummy Engine");
        CHECK(uciOutput[1].line == "id author Fastchess");
        CHECK(uciOutput[2].line == "option name Threads type spin default 1 min 1 max 1024");
        CHECK(uciOutput[3].line == "option name Hash type spin default 1 min 1 max 500000");
        CHECK(uciOutput[4].line == "option name MultiPV type spin default 1 min 1 max 256");
        CHECK(uciOutput[5].line == "option name UCI_Chess960 type check default false");
        CHECK(uciOutput[6].line == "line0");
        CHECK(uciOutput[7].line == "line1");
        CHECK(uciOutput[8].line == "uciok");
        CHECK(uci_engine.isready().code == engine::process::Status::OK);

        CHECK(uci_engine.writeEngine("sleep"));
        const auto res = uci_engine.readEngine("done", std::chrono::milliseconds(100));
        CHECK(res.code == engine::process::Status::TIMEOUT);

        CHECK(uci_engine.writeEngine("sleep"));
        const auto res2 = uci_engine.readEngine("done", std::chrono::milliseconds(5000));
        CHECK(res2.code == engine::process::Status::OK);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0].line == "done");
    }

    TEST_CASE("Testing the EngineProcess class with lower level class functions") {
        EngineConfiguration config;
        config.cmd = path;

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        CHECK(uci_engine.start(/*cpus*/ std::nullopt));

        CHECK(uci_engine.writeEngine("uci"));
        const auto res = uci_engine.readEngine("uciok");

        CHECK(res.code == engine::process::Status::OK);
        CHECK(uci_engine.output().size() == 9);
        CHECK(uci_engine.output()[0].line == "id name Dummy Engine");
        CHECK(uci_engine.output()[1].line == "id author Fastchess");
        CHECK(uci_engine.output()[2].line == "option name Threads type spin default 1 min 1 max 1024");
        CHECK(uci_engine.output()[3].line == "option name Hash type spin default 1 min 1 max 500000");
        CHECK(uci_engine.output()[4].line == "option name MultiPV type spin default 1 min 1 max 256");
        CHECK(uci_engine.output()[5].line == "option name UCI_Chess960 type check default false");
        CHECK(uci_engine.output()[6].line == "line0");
        CHECK(uci_engine.output()[7].line == "line1");
        CHECK(uci_engine.output()[8].line == "uciok");

        CHECK(uci_engine.writeEngine("isready"));
        const auto res2 = uci_engine.readEngine("readyok");
        CHECK(res2.code == engine::process::Status::OK);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0].line == "readyok");

        CHECK(uci_engine.writeEngine("sleep"));
        const auto res3 = uci_engine.readEngine("done", std::chrono::milliseconds(100));
        CHECK(res3.code == engine::process::Status::TIMEOUT);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        CHECK(uci_engine.writeEngine("sleep"));
        const auto res4 = uci_engine.readEngine("done", std::chrono::milliseconds(5000));
        CHECK(res4.code == engine::process::Status::OK);
        CHECK(uci_engine.output().size() == 1);
        CHECK(uci_engine.output()[0].line == "done");
    }

    TEST_CASE("Restarting the engine") {
        EngineConfiguration config;
        config.cmd = path;

        std::unique_ptr<engine::UciEngine> uci_engine = std::make_unique<MockUciEngine>(config, false);

        CHECK(uci_engine->start(/*cpus*/ std::nullopt));

        CHECK(uci_engine->writeEngine("uci"));
        const auto res = uci_engine->readEngine("uciok");

        CHECK(res.code == engine::process::Status::OK);
        CHECK(uci_engine->output().size() == 9);
        CHECK(uci_engine->output()[0].line == "id name Dummy Engine");
        CHECK(uci_engine->output()[1].line == "id author Fastchess");
        CHECK(uci_engine->output()[2].line == "option name Threads type spin default 1 min 1 max 1024");
        CHECK(uci_engine->output()[3].line == "option name Hash type spin default 1 min 1 max 500000");
        CHECK(uci_engine->output()[4].line == "option name MultiPV type spin default 1 min 1 max 256");
        CHECK(uci_engine->output()[5].line == "option name UCI_Chess960 type check default false");
        CHECK(uci_engine->output()[6].line == "line0");
        CHECK(uci_engine->output()[7].line == "line1");
        CHECK(uci_engine->output()[8].line == "uciok");

        uci_engine = std::make_unique<MockUciEngine>(config, false);

        CHECK(uci_engine->start(/*cpus*/ std::nullopt));

        CHECK(uci_engine->writeEngine("uci"));
        const auto res2 = uci_engine->readEngine("uciok");

        CHECK(res2.code == engine::process::Status::OK);
        CHECK(uci_engine->output().size() == 9);
        CHECK(uci_engine->output()[0].line == "id name Dummy Engine");
        CHECK(uci_engine->output()[1].line == "id author Fastchess");
        CHECK(uci_engine->output()[2].line == "option name Threads type spin default 1 min 1 max 1024");
        CHECK(uci_engine->output()[3].line == "option name Hash type spin default 1 min 1 max 500000");
        CHECK(uci_engine->output()[4].line == "option name MultiPV type spin default 1 min 1 max 256");
        CHECK(uci_engine->output()[5].line == "option name UCI_Chess960 type check default false");
        CHECK(uci_engine->output()[6].line == "line0");
        CHECK(uci_engine->output()[7].line == "line1");
        CHECK(uci_engine->output()[8].line == "uciok");
    }

    TEST_CASE("Restarting the engine") {
        EngineConfiguration config;
        config.cmd = path;

        std::unique_ptr<engine::UciEngine> uci_engine = std::make_unique<MockUciEngine>(config, false);

        CHECK(uci_engine->start(/*cpus*/ std::nullopt));

        CHECK(uci_engine->writeEngine("uci"));
        const auto res = uci_engine->readEngine("uciok");

        CHECK(res.code == engine::process::Status::OK);
        CHECK(uci_engine->output().size() == 9);
        CHECK(uci_engine->output()[0].line == "id name Dummy Engine");
        CHECK(uci_engine->output()[1].line == "id author Fastchess");
        CHECK(uci_engine->output()[2].line == "option name Threads type spin default 1 min 1 max 1024");
        CHECK(uci_engine->output()[3].line == "option name Hash type spin default 1 min 1 max 500000");
        CHECK(uci_engine->output()[4].line == "option name MultiPV type spin default 1 min 1 max 256");
        CHECK(uci_engine->output()[5].line == "option name UCI_Chess960 type check default false");
        CHECK(uci_engine->output()[6].line == "line0");
        CHECK(uci_engine->output()[7].line == "line1");
        CHECK(uci_engine->output()[8].line == "uciok");

        uci_engine = std::make_unique<MockUciEngine>(config, false);

        CHECK(uci_engine->start(/*cpus*/ std::nullopt));

        CHECK(uci_engine->writeEngine("uci"));
        const auto res2 = uci_engine->readEngine("uciok");

        CHECK(res2.code == engine::process::Status::OK);
        CHECK(uci_engine->output().size() == 9);
        CHECK(uci_engine->output()[0].line == "id name Dummy Engine");
        CHECK(uci_engine->output()[1].line == "id author Fastchess");
        CHECK(uci_engine->output()[2].line == "option name Threads type spin default 1 min 1 max 1024");
        CHECK(uci_engine->output()[3].line == "option name Hash type spin default 1 min 1 max 500000");
        CHECK(uci_engine->output()[4].line == "option name MultiPV type spin default 1 min 1 max 256");
        CHECK(uci_engine->output()[5].line == "option name UCI_Chess960 type check default false");
        CHECK(uci_engine->output()[6].line == "line0");
        CHECK(uci_engine->output()[7].line == "line1");
        CHECK(uci_engine->output()[8].line == "uciok");
    }

    TEST_CASE("Sending uci options and expect Threads to be first") {
        EngineConfiguration config;
        config.cmd     = path;
        config.options = {
            {"Hash", "1600"},
            {"MultiPV", "3"},
            {"UCI_Chess960", "true"},
            {"Threads", "4"},
        };

        std::unique_ptr<engine::UciEngine> uci_engine = std::make_unique<MockUciEngine>(config, false);

        CHECK(uci_engine->start(/*cpus*/ std::nullopt));

        CHECK(uci_engine->refreshUci());
        const auto res = uci_engine->readEngine("option set: setoption name UCI_Chess960");

        CHECK(res.code == engine::process::Status::OK);
        CHECK(uci_engine->output().size() == 4);
        CHECK(uci_engine->output()[0].line == "option set: setoption name Threads value 4");
        CHECK(uci_engine->output()[1].line == "option set: setoption name Hash value 1600");
        CHECK(uci_engine->output()[2].line == "option set: setoption name MultiPV value 3");
        CHECK(uci_engine->output()[3].line == "option set: setoption name UCI_Chess960 value true");
    }
}
