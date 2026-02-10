#include <matchmaking/player.hpp>

#include <doctest/doctest.hpp>

const std::string path = "./app/tests/mock/engine/";

namespace fastchess {
TEST_SUITE("Player Test") {
    TEST_CASE("Test Player Constructor") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = path + "dummy_engine.exe";
#else
        config.cmd = path + "dummy_engine";
#endif
        config.args = "arg1 arg2 arg3";

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        Player player = Player(uci_engine);

        CHECK(player.engine.getConfig().cmd == config.cmd);
        CHECK(player.engine.getConfig().args == config.args);
    }

    TEST_CASE("Test Player Fixed Time") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = path + "dummy_engine.exe";
#else
        config.cmd = path + "dummy_engine";
#endif
        config.args = "arg1 arg2 arg3";

        config.limit.tc = TimeControl::Limits();

        config.limit.tc.fixed_time = 1000;
        config.limit.tc.increment  = 0;
        config.limit.tc.time       = 0;
        config.limit.tc.moves      = 0;
        config.limit.tc.timemargin = 0;

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        Player player = Player(uci_engine);

        CHECK(player.getTimeControl().isFixedTime() == true);
        CHECK(player.getTimeControl().getTimeoutThreshold() == std::chrono::milliseconds(1000 + TimeControl::MARGIN));

        CHECK(player.getTimeControl().updateTime(1000) == true);
        CHECK(player.getTimeControl().getTimeLeft() == 1000);

        CHECK(player.getTimeControl().updateTime(1000) == true);
        CHECK(player.getTimeControl().getTimeLeft() == 1000);
    }

    TEST_CASE("Test Player Timed") {
        EngineConfiguration config;
#ifdef _WIN64
        config.cmd = path + "dummy_engine.exe";
#else
        config.cmd = path + "dummy_engine";
#endif
        config.args = "arg1 arg2 arg3";

        config.limit.tc = TimeControl::Limits();

        config.limit.tc.fixed_time = 0;
        config.limit.tc.increment  = 0;
        config.limit.tc.time       = 1000;
        config.limit.tc.moves      = 0;
        config.limit.tc.timemargin = 0;

        engine::UciEngine uci_engine = engine::UciEngine(config, false);

        Player player = Player(uci_engine);

        CHECK(player.getTimeControl().isFixedTime() == false);
        CHECK(player.getTimeControl().getTimeoutThreshold() == std::chrono::milliseconds(1000 + TimeControl::MARGIN));

        CHECK(player.getTimeControl().updateTime(1000) == true);
        CHECK(player.getTimeControl().getTimeLeft() == 0);

        CHECK(player.getTimeControl().updateTime(1000) == false);
        CHECK(player.getTimeControl().getTimeLeft() == -1000);
    }
}
}  // namespace fastchess
