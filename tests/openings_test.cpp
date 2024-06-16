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

        auto book = book::OpeningBook(tournament);
        std::vector<std::string> epd = book.getEpdBook();

        std::string fen = epd[0];

        CHECK(epd.size() == 10);
        CHECK(epd.capacity() == 10);
        CHECK(fen == "r1b1k1nr/pp1nqppp/2p5/4p1bQ/2B1P3/8/PPP2PPP/RNB2RK1 w kq - 2 9");
    }
}

