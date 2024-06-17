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
        tournament.opening.order = OrderType::RANDOM;
        tournament.opening.start = 3256;
        tournament.seed = 123456789;

        auto book = book::OpeningBook(tournament);
        std::vector<std::string> epd = book.getEpdBook();

        std::string fen = epd[0];

        CHECK(epd.size() == 10);
        CHECK(epd.capacity() == 10);
        CHECK(fen == "rnbq1rk1/1p3pbp/p2ppnp1/2pP4/P1P1P3/2N2N1P/1P3PP1/R1BQKB1R w KQ - 0 9");
    }
}

