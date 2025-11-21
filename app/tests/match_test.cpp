#include <matchmaking/match/match.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("Match Test") {
    TEST_CASE("convertScoreToString") {
        CHECK(Match::convertScoreToString(0, engine::ScoreType::CP) == "0.00");
        CHECK(Match::convertScoreToString(100, engine::ScoreType::CP) == "+1.00");
        CHECK(Match::convertScoreToString(-100, engine::ScoreType::CP) == "-1.00");
        CHECK(Match::convertScoreToString(100, engine::ScoreType::MATE) == "+M199");
        CHECK(Match::convertScoreToString(-100, engine::ScoreType::MATE) == "-M200");
        CHECK(Match::convertScoreToString(1, engine::ScoreType::MATE) == "+M1");
        CHECK(Match::convertScoreToString(-1, engine::ScoreType::MATE) == "-M2");
    }

    TEST_CASE("DrawTracker") {
        SUBCASE("adjudicates when conditions met") {
            DrawTracker dt(10, 3, 50);
            for (int i = 0; i < 6; ++i) dt.update({engine::ScoreType::CP, 40}, 1);
            CHECK(dt.adjudicatable(10));
        }

        SUBCASE("does not adjudicate before move_number") {
            DrawTracker dt(10, 3, 50);
            for (int i = 0; i < 6; ++i) dt.update({engine::ScoreType::CP, 40}, 1);
            CHECK_FALSE(dt.adjudicatable(9));
        }

        SUBCASE("hmvc resets counter") {
            DrawTracker dt(10, 3, 50);
            dt.update({engine::ScoreType::CP, 40}, 0);
            dt.update({engine::ScoreType::CP, 40}, 1);
            CHECK_FALSE(dt.adjudicatable(10));
        }

        SUBCASE("non-CP score does not count") {
            DrawTracker dt(10, 1, 0);
            dt.update({engine::ScoreType::MATE, 0}, 1);
            CHECK_FALSE(dt.adjudicatable(10));
        }

        SUBCASE("default parameters") {
            DrawTracker dt(0, 1, 0);
            dt.update({engine::ScoreType::CP, 0}, 1);
            dt.update({engine::ScoreType::CP, 0}, 1);
            CHECK(dt.adjudicatable(0));
        }
    }

    TEST_CASE("ResignTracker") {
        SUBCASE("twosided resigns after move_count") {
            ResignTracker rt(100, 2, true);
            rt.update({engine::ScoreType::CP, 150}, chess::Color::WHITE);
            rt.update({engine::ScoreType::CP, -150}, chess::Color::BLACK);
            rt.update({engine::ScoreType::CP, 150}, chess::Color::WHITE);
            rt.update({engine::ScoreType::CP, -150}, chess::Color::BLACK);
            CHECK(rt.resignable());
        }

        SUBCASE("twosided resets on non-qualifying score") {
            ResignTracker rt(100, 2, true);
            rt.update({engine::ScoreType::CP, 150}, chess::Color::WHITE);
            rt.update({engine::ScoreType::CP, 50}, chess::Color::BLACK);
            CHECK_FALSE(rt.resignable());
        }

        SUBCASE("non-twosided white resigns") {
            ResignTracker rt(100, 2, false);
            rt.update({engine::ScoreType::CP, -150}, chess::Color::WHITE);
            rt.update({engine::ScoreType::CP, -150}, chess::Color::WHITE);
            CHECK(rt.resignable());
        }

        SUBCASE("non-twosided black resigns") {
            ResignTracker rt(100, 2, false);
            rt.update({engine::ScoreType::CP, -150}, chess::Color::BLACK);
            rt.update({engine::ScoreType::CP, -150}, chess::Color::BLACK);
            CHECK(rt.resignable());
        }

        SUBCASE("mate score triggers resign") {
            ResignTracker rt(0, 1, false);
            rt.update({engine::ScoreType::MATE, -1}, chess::Color::WHITE);
            CHECK(rt.resignable());
        }
    }

    TEST_CASE("MaxMovesTracker") {
        SUBCASE("reaches max moves") {
            MaxMovesTracker mt(50);
            for (int i = 0; i < 100; ++i) mt.update();
            CHECK(mt.maxmovesreached());
        }

        SUBCASE("does not reach before") {
            MaxMovesTracker mt(50);
            for (int i = 0; i < 99; ++i) mt.update();
            CHECK_FALSE(mt.maxmovesreached());
        }
    }
}

}  // namespace fastchess
