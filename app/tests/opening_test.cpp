#include <game/book/opening_book.hpp>
#include <types/tournament.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess;

TEST_SUITE("Opening Book Test") {
    TEST_CASE("Test fewer openings than rounds") {
        config::Tournament tournament;
        tournament.rounds         = 10;
        tournament.opening.file   = "app/tests/data/test.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::SEQUENTIAL;
        tournament.opening.start  = 3256;

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 2);
        CHECK(book[id].fen_epd == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 2);
        CHECK(book[id].fen_epd == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 2);
        CHECK(book[id].fen_epd == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");
    }

    TEST_CASE("Test equal openings to rounds") {
        config::Tournament tournament;
        tournament.rounds         = 3;
        tournament.opening.file   = "app/tests/data/test.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::SEQUENTIAL;
        tournament.opening.start  = 3256;

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 2);
        CHECK(book[id].fen_epd == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");
    }

    TEST_CASE("Test more openings than rounds") {
        config::Tournament tournament;
        tournament.rounds         = 2;
        tournament.opening.file   = "app/tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::SEQUENTIAL;
        tournament.opening.start  = 3256;

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "r1b1kb1r/1p2pppp/p1np4/q5B1/3NP1n1/2N4P/PPP2PP1/R2QKB1R w KQkq - 1 9");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "r1bqkb1r/1p3ppp/p1np1n2/4p3/3NPP2/2N1BQ2/PPP3PP/R3KB1R w KQkq - 0 9");
    }

    TEST_CASE("Test more openings than rounds, Initial Matchcount 1") {
        config::Tournament tournament;
        tournament.rounds         = 2;
        tournament.opening.file   = "app/tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::SEQUENTIAL;
        tournament.opening.start  = 1;

        auto book = book::OpeningBook(tournament, 1);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9");
    }

    TEST_CASE("Test more openings than rounds, Initial Matchcount 2") {
        config::Tournament tournament;
        tournament.rounds         = 2;
        tournament.opening.file   = "app/tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::SEQUENTIAL;
        tournament.opening.start  = 1;

        auto book = book::OpeningBook(tournament, 2);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "rnb2rk1/ppp2pbp/3p2p1/3Pp2n/2P1P2q/2N1BP2/PP1Q2PP/R3KBNR w KQ - 3 9");
    }

    TEST_CASE("Test fewer openings than rounds random") {
        config::Tournament tournament;
        tournament.rounds         = 4;
        tournament.opening.file   = "app/tests/data/test.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::RANDOM;
        tournament.opening.start  = 3256;
        tournament.seed           = 123456789;

        util::random::seed(tournament.seed);

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");
    }

    TEST_CASE("Test equal openings to rounds random") {
        config::Tournament tournament;
        tournament.rounds         = 3;
        tournament.opening.file   = "app/tests/data/test.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::RANDOM;
        tournament.opening.start  = 3256;
        tournament.seed           = 123456789;

        util::random::seed(tournament.seed);

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 2);
        CHECK(book[id].fen_epd == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");
    }

    TEST_CASE("Test more openings than rounds random") {
        config::Tournament tournament;
        tournament.rounds         = 2;
        tournament.opening.file   = "app/tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order  = OrderType::RANDOM;
        tournament.opening.start  = 3256;
        tournament.seed           = 123456789;

        util::random::seed(tournament.seed);

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == "1n1qkb1r/rp3ppp/p1p1pn2/2PpN2b/3PP3/1QN5/PP3PPP/R1B1KB1R w KQk - 0 9");

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == "rnbqkb1r/5ppp/p2p1n2/2pP4/Pp2P3/5NP1/1P3P1P/RNBQKB1R w KQkq - 0 9");
    }

    TEST_CASE("Test with PGN opening book") {
        config::Tournament tournament;
        tournament.rounds         = 2;
        tournament.opening.file   = "app/tests/data/test.pgn";
        tournament.opening.format = FormatType::PGN;
        tournament.opening.order  = OrderType::SEQUENTIAL;
        tournament.opening.start  = 1;

        auto book = book::OpeningBook(tournament);
        auto id   = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 0);
        CHECK(book[id].fen_epd == chess::constants::STARTPOS);
        CHECK(book[id].moves.size() == 16);
        CHECK(book[id].stm == chess::Color::WHITE);
        CHECK(book[id].moves[0] == chess::Move::make(chess::Square::SQ_E2, chess::Square::SQ_E4));

        id = book.fetchId();

        REQUIRE(id.has_value());
        CHECK(id.value() == 1);
        CHECK(book[id].fen_epd == chess::constants::STARTPOS);
        CHECK(book[id].moves.size() == 16);
        CHECK(book[id].stm == chess::Color::WHITE);
    }
}
