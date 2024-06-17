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

        CHECK(epd.size() == 10);
        CHECK(epd.capacity() == 10);
        CHECK(epd[0] == "1n1qkb1r/rp3ppp/p1p1pn2/2PpN2b/3PP3/1QN5/PP3PPP/R1B1KB1R w KQk - 0 9");
        CHECK(epd[9] == "rn1q1rk1/p2pbppp/bp3n2/2pp4/2P1P3/1QN2NP1/PP3P1P/R1B1KB1R w KQ - 0 9");
    }
}

