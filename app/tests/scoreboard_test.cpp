#include <matchmaking/scoreboard.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("ScoreBoard") {
    TEST_CASE("Update and Get") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        const auto stats = Stats(1, 2, 3);

        ScoreBoard scoreboard;
        scoreboard.updateNonPair({engine1, engine2}, stats);

        CHECK(scoreboard.getStats(engine1.name, engine2.name) == stats);
    }

    TEST_CASE("Update and Update and Get") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        auto stats = Stats(1, 2, 3);

        ScoreBoard scoreboard;
        scoreboard.updateNonPair({engine1, engine2}, stats);
        scoreboard.updateNonPair({engine1, engine2}, stats);

        CHECK(scoreboard.getStats(engine1.name, engine2.name) == Stats{2, 4, 6});
    }

    TEST_CASE("Update and Update Reverse and Get") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        ScoreBoard scoreboard;

        const auto stats = Stats{1, 2, 3};

        scoreboard.updateNonPair({engine1, engine2}, stats);
        scoreboard.updateNonPair({engine2, engine1}, stats);

        CHECK(scoreboard.getStats(engine1.name, engine2.name) == Stats(3, 3, 6));
        CHECK(scoreboard.getStats(engine2.name, engine1.name) == Stats(3, 3, 6));
    }

    TEST_CASE("Update with black-to-move opening records assignment perspective") {
        EngineConfiguration engine1 = {};
        EngineConfiguration engine2 = {};

        engine1.name = "engine1";
        engine2.name = "engine2";

        MatchData match_data;
        match_data.players.white = {engine2, chess::GameResult::LOSE, false};
        match_data.players.black = {engine1, chess::GameResult::WIN, true};

        ScoreBoard scoreboard;
        scoreboard.updateNonPair({engine2, engine1}, Stats(match_data));

        CHECK(scoreboard.getStats(engine1.name, engine2.name) == Stats(1, 0, 0));
    }

    // TEST_CASE("SetResults") {
    //     EngineConfiguration engine1 = {};
    //     EngineConfiguration engine2 = {};

    //     engine1.name = "engine1";
    //     engine2.name = "engine2";

    //     stats_map results = {{engine1.name, {{engine2.name, Stats(1, 2, 3)}}},
    //                          {engine2.name, {{engine1.name, Stats(0, 0, 0)}}}};

    //     ScoreBoard result;
    //     result.setResults(results);

    //     CHECK(result.getStats(engine1.name, engine2.name) == Stats(1, 2, 3));
    //     CHECK(result.getResults() == results);
    // }
}
}  // namespace fastchess
