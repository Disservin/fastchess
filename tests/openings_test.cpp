#include <types/tournament_options.hpp>
#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
    TEST_CASE("check epd openings") {
        options::Tournament tournament;
        tournament.rounds = 3000;
        tournament.opening.file = "tests/data/openings.epd";
        tournament.seed = 123456789;
        tournament.opening.start = 3256;

        auto book = book::OpeningBook(tournament);
        std::vector<std::string> epd = book.getEpdBook();

        std::string fen = epd[0];

        CHECK(fen == "1n1qkb1r/rp3ppp/p1p1pn2/2PpN2b/3PP3/1QN5/PP3PPP/R1B1KB1R w KQk - 0 9");
    }
}

