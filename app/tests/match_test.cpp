#include <matchmaking/match/match.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("Match Test") {
    TEST_CASE("DrawTracker") {
        SUBCASE("adjudicates when conditions met") {
            DrawTracker dt(10, 3, 50);
            for (int i = 0; i < 6; ++i) dt.update({ScoreType::CP, 40}, 1);
            CHECK(dt.adjudicatable(10));
        }

        SUBCASE("does not adjudicate before move_number") {
            DrawTracker dt(10, 3, 50);
            for (int i = 0; i < 6; ++i) dt.update({ScoreType::CP, 40}, 1);
            CHECK_FALSE(dt.adjudicatable(9));
        }

        SUBCASE("hmvc resets counter") {
            DrawTracker dt(10, 3, 50);
            dt.update({ScoreType::CP, 40}, 0);
            dt.update({ScoreType::CP, 40}, 1);
            CHECK_FALSE(dt.adjudicatable(10));
        }

        SUBCASE("non-CP score does not count") {
            DrawTracker dt(10, 1, 0);
            dt.update({ScoreType::MATE, 0}, 1);
            CHECK_FALSE(dt.adjudicatable(10));
        }

        SUBCASE("default parameters") {
            DrawTracker dt(0, 1, 0);
            dt.update({ScoreType::CP, 0}, 1);
            dt.update({ScoreType::CP, 0}, 1);
            CHECK(dt.adjudicatable(0));
        }
    }

    TEST_CASE("TimeoutReason") {
        CHECK(formatTimeoutReason("White", 0) == "White loses on time (0ms overrun)");
        CHECK(formatTimeoutReason("Black", 1234) == "Black loses on time (1234ms overrun)");
    }

    TEST_CASE("PV checks") {
        SUBCASE("ignores info without pv") {
            CHECK_FALSE(checkPvLine(chess::Board(), "info depth 1 score cp 10", true).has_value());
        }

        SUBCASE("accepts legal pv") {
            CHECK_FALSE(checkPvLine(chess::Board(), "info depth 1 score cp 10 pv e2e4 e7e5", true).has_value());
        }

        SUBCASE("reports illegal pv move") {
            const auto result = checkPvLine(chess::Board(), "info depth 1 score cp 10 pv e2e5", true);

            REQUIRE(result.has_value());
            CHECK(result->warning == PvWarning::IllegalMove);
            CHECK(result->move == "e2e5");
        }

        SUBCASE("skips mate pv checks when disabled") {
            CHECK_FALSE(checkPvLine(chess::Board(), "info depth 1 score mate 3 pv e2e4", false).has_value());
        }

        SUBCASE("reports incomplete mating pv") {
            const auto result = checkPvLine(chess::Board(), "info depth 1 score mate 3 pv e2e4", true);

            REQUIRE(result.has_value());
            CHECK(result->warning == PvWarning::IncompleteMatingPv);
        }

        SUBCASE("reports too long mating pv") {
            const auto result = checkPvLine(chess::Board(), "info depth 1 score mate -1 pv f2f3 e7e5 g2g4", true);

            REQUIRE(result.has_value());
            CHECK(result->warning == PvWarning::TooLongMatingPv);
        }

        SUBCASE("reports mating pv that does not end in mate") {
            const auto result = checkPvLine(chess::Board(), "info depth 1 score mate -1 pv f2f3 e7e5", true);

            REQUIRE(result.has_value());
            CHECK(result->warning == PvWarning::MatingPvDoesNotEndWithCheckmate);
        }

        SUBCASE("accepts complete mating pv") {
            const auto result = checkPvLine(chess::Board(), "info depth 1 score mate -2 pv f2f3 e7e5 g2g4 d8h4", true);

            CHECK_FALSE(result.has_value());
        }

        SUBCASE("reports pv continuing after checkmate") {
            const auto result =
                checkPvLine(chess::Board(), "info depth 1 score cp 10 pv f2f3 e7e5 g2g4 d8h4 a2a3", true);

            REQUIRE(result.has_value());
            CHECK(result->warning == PvWarning::ContinuesAfterCheckmate);
            CHECK(result->move == "a2a3");
        }
    }

    TEST_CASE("Bestmove PV check") {
        CHECK_FALSE(checkBestmovePv("info depth 1 score cp 10 pv e2e4 e7e5", "e2e4").has_value());
        CHECK_FALSE(checkBestmovePv("info depth 1 score cp 10 lowerbound pv e2e4", "d2d4").has_value());
        CHECK_FALSE(checkBestmovePv("info depth 1 score cp 10", "e2e4").has_value());

        const auto result = checkBestmovePv("info depth 1 score cp 10 pv e2e4 e7e5", "d2d4");

        REQUIRE(result.has_value());
        CHECK(result->warning == PvWarning::BestmoveMismatch);
        CHECK(result->move == "d2d4");
    }

    TEST_CASE("ResignTracker") {
        SUBCASE("twosided resigns after move_count") {
            ResignTracker rt(100, 2, true);
            rt.update({ScoreType::CP, 150}, chess::Color::WHITE);
            rt.update({ScoreType::CP, -150}, chess::Color::BLACK);
            rt.update({ScoreType::CP, 150}, chess::Color::WHITE);
            rt.update({ScoreType::CP, -150}, chess::Color::BLACK);
            CHECK(rt.resignable());
        }

        SUBCASE("twosided resets on non-qualifying score") {
            ResignTracker rt(100, 2, true);
            rt.update({ScoreType::CP, 150}, chess::Color::WHITE);
            rt.update({ScoreType::CP, 50}, chess::Color::BLACK);
            CHECK_FALSE(rt.resignable());
        }

        SUBCASE("non-twosided white resigns") {
            ResignTracker rt(100, 2, false);
            rt.update({ScoreType::CP, -150}, chess::Color::WHITE);
            rt.update({ScoreType::CP, -150}, chess::Color::WHITE);
            CHECK(rt.resignable());
        }

        SUBCASE("non-twosided black resigns") {
            ResignTracker rt(100, 2, false);
            rt.update({ScoreType::CP, -150}, chess::Color::BLACK);
            rt.update({ScoreType::CP, -150}, chess::Color::BLACK);
            CHECK(rt.resignable());
        }

        SUBCASE("mate score triggers resign") {
            ResignTracker rt(0, 1, false);
            rt.update({ScoreType::MATE, -1}, chess::Color::WHITE);
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
