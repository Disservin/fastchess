#include <book/epd_reader.hpp>

#include <chess.hpp>
#include "doctest/doctest.hpp"

namespace fastchess {

TEST_SUITE("EPD Reader") {
    TEST_CASE("Read EPD file") {
        book::EpdReader reader("app/tests/data/test.epd");

        const auto games = reader.get();

        CHECK(games.size() == 3);

        CHECK(games[0] == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");
        CHECK(games[1] == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");
        CHECK(games[2] == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");
    }

    TEST_CASE("Read EPD file with invalid file") {
        book::EpdReader reader("app/tests/data/das.epd");

        CHECK_THROWS_WITH_AS(static_cast<void>(reader.get()), "No openings found in file: app/tests/data/das.epd",
                             std::runtime_error);
    }
}

}  // namespace fastchess