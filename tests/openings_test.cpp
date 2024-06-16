#include <types/tournament_options.hpp>
#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
    TEST_CASE("check epd openings") {
        options::Tournament tournament;
        tournament.rounds = 3000;
        tournament.opening.file = "./data/openings.epd"; // Ensure the file path is set within opening
        tournament.seed = 123456789;
        tournament.opening.start = 3256;

        auto book = book::OpeningBook(tournament);
        auto epd = book.getEpdBook();

        auto fen1 = epd[1];

        //CHECK(1 == 1);
        CHECK(epd[1] == "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9");
    }
}

