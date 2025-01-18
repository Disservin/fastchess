#include <matchmaking/match/match.hpp>

#include "doctest/doctest.hpp"

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
}
}  // namespace fastchess
