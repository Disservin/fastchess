#include <pgn_reader.hpp>

#include <cassert>
#include <chrono>
#include <thread>

#include "doctest/doctest.hpp"

namespace fast_chess {
TEST_SUITE("PgnReader") {
    TEST_CASE("PgnReader") {
        PgnReader reader("tests/data/test.pgn");
        const auto games = reader.getPgns();

        for (auto opening : games) {
            std::cout << opening.fen << std::endl;
            for (auto move : opening.moves) {
                std::cout << move << std::endl;
            }
        }

        CHECK(games.size() == 6);

        CHECK(games[0].fen == Chess::STARTPOS);
        CHECK(games[1].fen == Chess::STARTPOS);
        CHECK(games[2].fen == Chess::STARTPOS);
        CHECK(games[3].fen == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");
        CHECK(games[4].fen == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");
        CHECK(games[5].fen == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");

        CHECK(games[0].moves.size() == 16);
        CHECK(games[1].moves.size() == 16);
        CHECK(games[2].moves.size() == 16);
        CHECK(games[3].moves.size() == 0);
        CHECK(games[4].moves.size() == 0);
        CHECK(games[5].moves.size() == 0);

        const std::vector<std::string> moves = {"e2e4", /*...*/};
        CHECK(games[0].moves == moves);
    }
}
}  // namespace fast_chess
