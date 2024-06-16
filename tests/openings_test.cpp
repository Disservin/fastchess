#include <types/tournament_options.hpp>
#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
    TEST_CASE("check epd openings") {
        options::Tournament tournament;
        tournament.rounds = 10;
        tournament.opening.file = "tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order = OrderType::SEQUENTIAL;
        tournament.opening.start = 3256;

        auto book = book::OpeningBook(tournament);
        std::vector<std::string> epd = book.getEpdBook();

        std::string fen = epd[0];

        CHECK(epd.size() == 10);
        CHECK(epd.capacity() == 10);
        CHECK(fen == "r1b1kb1r/1p2pppp/p1np4/q5B1/3NP1n1/2N4P/PPP2PP1/R2QKB1R w KQkq - 1 9");
    }
}

