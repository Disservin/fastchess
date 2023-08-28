#include <matchmaking/result.hpp>

#include <cassert>
#include <chrono>
#include <thread>

#include "doctest/doctest.hpp"

namespace fast_chess {

TEST_SUITE("Result Tests") {
    TEST_CASE("Update and Get") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        Stats stats = Stats{.wins = 1, .losses = 2, .draws = 3};

        Result result;
        result.updateStats({engine1, engine2}, stats);

        CHECK(result.getStats(engine1.name, engine2.name) == stats);
    }

    TEST_CASE("Update and Update and Get") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        Stats stats = Stats{.wins = 1, .losses = 2, .draws = 3};

        Result result;
        result.updateStats({engine1, engine2}, stats);
        result.updateStats({engine1, engine2}, stats);

        CHECK(result.getStats(engine1.name, engine2.name) ==
              Stats{.wins = 2, .losses = 4, .draws = 6});
    }

    TEST_CASE("Update and Update Reverse and Get") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        Result result;

        Stats stats = Stats{.wins = 1, .losses = 2, .draws = 3};

        result.updateStats({engine1, engine2}, stats);
        result.updateStats({engine2, engine1}, stats);

        CHECK(result.getStats(engine1.name, engine2.name) ==
              Stats{.wins = 3, .losses = 3, .draws = 6});
        CHECK(result.getStats(engine2.name, engine1.name) ==
              Stats{.wins = 3, .losses = 3, .draws = 6});
    }

    TEST_CASE("SetResults") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        stats_map results = {
            {engine1.name, {{engine2.name, Stats{.wins = 1, .losses = 2, .draws = 3}}}},
            {engine2.name, {{engine1.name, Stats{.wins = 0, .losses = 0, .draws = 0}}}}};

        Result result;
        result.setResults(results);

        CHECK(result.getStats(engine1.name, engine2.name) ==
              Stats{.wins = 1, .losses = 2, .draws = 3});
        CHECK(result.getResults() == results);
    }
}
}  // namespace fast_chess
