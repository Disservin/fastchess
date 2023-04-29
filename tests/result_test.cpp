#include "../src/matchmaking/result.hpp"

#include <cassert>
#include <chrono>
#include <thread>

#include "doctest/doctest.hpp"

namespace fast_chess {
TEST_SUITE("Result Tests") {
    std::string engine1 = "engine1";
    std::string engine2 = "engine2";

    TEST_CASE("Update and Get") {
        Result result;

        Stats stats = Stats{.wins = 1, .losses = 2, .draws = 3};
        result.updateStats(engine1, engine2, stats);
        CHECK(result.getStats(engine1, engine2) == stats);
    }

    TEST_CASE("Update and Update and Get") {
        Result result;

        Stats stats = Stats{.wins = 1, .losses = 2, .draws = 3};
        result.updateStats(engine1, engine2, stats);
        result.updateStats(engine1, engine2, stats);
        CHECK(result.getStats(engine1, engine2) == Stats{.wins = 2, .losses = 4, .draws = 6});
    }

    TEST_CASE("Update and Update Reverse and Get") {
        Result result;

        Stats stats = Stats{.wins = 1, .losses = 2, .draws = 3};
        result.updateStats(engine1, engine2, stats);
        result.updateStats(engine2, engine1, stats);
        CHECK(result.getStats(engine1, engine2) == Stats{.wins = 3, .losses = 3, .draws = 6});
        CHECK(result.getStats(engine2, engine1) == Stats{.wins = 3, .losses = 3, .draws = 6});
    }

    TEST_CASE("SetResults") {
        stats_map results = {{engine1, {{engine2, Stats{.wins = 1, .losses = 2, .draws = 3}}}},
                             {engine2, {{engine1, Stats{.wins = 0, .losses = 0, .draws = 0}}}}};
        Result result;
        result.setResults(results);
        CHECK(result.getStats(engine1, engine2) == Stats{.wins = 1, .losses = 2, .draws = 3});
        CHECK(result.getResults() == results);
    }
}
}  // namespace fast_chess
