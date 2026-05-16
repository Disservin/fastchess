#include <matchmaking/match/match.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("Score Test") {
    TEST_CASE("convertScoreToString") {
        CHECK(static_cast<std::string>(Score{ScoreType::CP, 0}) == "0.00");
        CHECK(static_cast<std::string>(Score{ScoreType::CP, 100}) == "+1.00");
        CHECK(static_cast<std::string>(Score{ScoreType::CP, -100}) == "-1.00");
        CHECK(static_cast<std::string>(Score{ScoreType::MATE, 100}) == "+M199");
        CHECK(static_cast<std::string>(Score{ScoreType::MATE, -100}) == "-M200");
        CHECK(static_cast<std::string>(Score{ScoreType::MATE, 1}) == "+M1");
        CHECK(static_cast<std::string>(Score{ScoreType::MATE, -1}) == "-M2");
    }
}

}  // namespace fastchess
