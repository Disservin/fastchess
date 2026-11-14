#include <game/book/epd_reader.hpp>
#include <types/exception.hpp>

#include <memory>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("EPD Reader") {
    TEST_CASE("Read EPD file") {
        book::EpdReader reader("app/tests/data/test.epd");

        const auto games = reader.get();

        CHECK(games.size() == 3);

        CHECK(games[0].view() == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");
        CHECK(games[1].view() == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");
        CHECK(games[2].view() == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");
    }

    TEST_CASE("Read EPD file with invalid lines") {
        // A line whose position can't be read or played from (an unknown side to move or piece, two white kings,
        // the side that just moved in check, a pawn on the last rank, three pieces giving check) is left out.
        book::EpdReader reader("app/tests/data/invalid.epd");

        const auto games = reader.get();

        REQUIRE(games.size() == 3);
        CHECK(reader.skipped() == 6);

        CHECK(games[0].view() == "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
        CHECK(games[1].view() == "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - hmvc 3; fmvn 3;");
        CHECK(games[2].view() == "8/8/8/4k3/8/8/4P3/4K3 w - -");
    }

    TEST_CASE("Read EPD file with invalid file") {
        std::unique_ptr<book::EpdReader> reader;

        CHECK_THROWS_WITH_AS(reader = std::make_unique<book::EpdReader>("app/tests/data/das.epd"),
                             "Failed to open file: app/tests/data/das.epd", fastchess_exception);
    }
}

}  // namespace fastchess