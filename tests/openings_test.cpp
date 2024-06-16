#include <types/tournament_options.hpp>
#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
    TEST_CASE("check epd openings") {
        options::Tournament tournament;
        tournament.rounds = 3000;
        tournament.opening.file = "./data/openings.epd";
        tournament.seed = 123456789;
        tournament.opening.start = 3256;

        auto book = book::OpeningBook(tournament);
        std::vector<std::string> epd = book.getEpdBook();

        std::string fen = epd[0];

        CHECK(fen == "r1bqkb1r/3p1ppp/p1p1p3/3nP3/8/2N5/PPP1BPPP/R1BQK2R w KQkq - 1 9");
    }
}

