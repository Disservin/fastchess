#include <matchmaking/player.hpp>

#include "doctest/doctest.hpp"

namespace fast_chess {
TEST_SUITE("Player Test") {
    TEST_CASE("Test buildPositionInput") {
        const auto fen =
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - "
            "0 1";

        CHECK(Player::buildPositionInput({}, "startpos") == "position startpos");

        CHECK(Player::buildPositionInput({"e2e4", "e7e5"}, "startpos") ==
              "position startpos moves e2e4 e7e5");

        CHECK(Player::buildPositionInput({}, fen) ==
              "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        CHECK(Player::buildPositionInput({"e2e4", "e7e5"}, fen) ==
              "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 moves e2e4 "
              "e7e5");
    }
}
}  // namespace fast_chess
