#include <types/tournament_options.hpp>
#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
    TEST_CASE("EPD 1") {
        options::Tournament tournament;
        tournament.rounds = 10;
        tournament.opening.file = "tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order = OrderType::RANDOM;
        tournament.opening.start = 3256;
        tournament.seed = 123456789;
        int initial_matchcount = 47;

        auto book = book::OpeningBook(tournament, initial_matchcount);
        std::vector<std::string> book_vector = book.getEpdBook();
        std::vector<pgn::Opening> opening(tournament.rounds);

        for (int i=0; i < tournament.rounds; i++) {
            opening[i] = book[book.fetchId()];
        }

        CHECK(book_vector.size() == 10);
        CHECK(book_vector.capacity() == 10);
        CHECK(opening[0].fen == "rn1qkb1r/1p3p1p/p2p1np1/2pP4/4P1b1/2N2N2/PP2QPPP/R1B1KB1R w KQkq - 2 9");
        CHECK(opening[9].fen == "rnbqk2r/pp2p1bp/2pp1np1/5P2/3P4/2N3P1/PPP2PB1/R1BQK1NR w KQkq - 3 9");
    }

    TEST_CASE("EPD 2") {
        options::Tournament tournament;
        tournament.rounds = 100;
        tournament.opening.file = "tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order = OrderType::RANDOM;
        tournament.opening.start = 3256;
        tournament.seed = 123456789;
        int initial_matchcount = 47;

        auto book = book::OpeningBook(tournament, initial_matchcount);
        std::vector<std::string> book_vector = book.getEpdBook();
        std::vector<pgn::Opening> opening(tournament.rounds);

        for (int i=0; i < tournament.rounds; i++) {
            opening[i] = book[book.fetchId()];
        }

        CHECK(book_vector.size() == 100);
        CHECK(book_vector.capacity() == 100);
        CHECK(opening[0].fen == "rn1qkb1r/1p3p1p/p2p1np1/2pP4/4P1b1/2N2N2/PP2QPPP/R1B1KB1R w KQkq - 2 9");
        CHECK(opening[9].fen == "rnbqk2r/pp2p1bp/2pp1np1/5P2/3P4/2N3P1/PPP2PB1/R1BQK1NR w KQkq - 3 9");
    }

    TEST_CASE("EPD 3") {
        options::Tournament tournament;
        tournament.rounds = 678;
        tournament.opening.file = "tests/data/openings.epd";
        tournament.opening.format = FormatType::EPD;
        tournament.opening.order = OrderType::RANDOM;
        tournament.opening.start = 3256;
        tournament.seed = 123456789;
        int initial_matchcount = 47;

        auto book = book::OpeningBook(tournament, initial_matchcount);
        std::vector<std::string> book_vector = book.getEpdBook();
        std::vector<pgn::Opening> opening(tournament.rounds);

        for (int i=0; i < tournament.rounds; i++) {
            opening[i] = book[book.fetchId()];
        }

        CHECK(book_vector.size() == 100);
        CHECK(book_vector.capacity() == 100);
        CHECK(opening[0].fen == "rn1qkb1r/1p3p1p/p2p1np1/2pP4/4P1b1/2N2N2/PP2QPPP/R1B1KB1R w KQkq - 2 9");
        CHECK(opening[9].fen == "rnbqk2r/pp2p1bp/2pp1np1/5P2/3P4/2N3P1/PPP2PB1/R1BQK1NR w KQkq - 3 9");
    }

    TEST_CASE("PGN 1") {
        options::Tournament tournament;
        tournament.rounds = 10;
        tournament.opening.file = "tests/data/test2.pgn";
        tournament.opening.format = FormatType::PGN;
        tournament.opening.order = OrderType::RANDOM;
        tournament.opening.start = 3256;
        tournament.seed = 123456789;
        int initial_matchcount = 0;

        auto book = book::OpeningBook(tournament, initial_matchcount);
        std::vector<pgn::Opening> book_vector = book.getPgnBook();
        std::vector<pgn::Opening> opening(tournament.rounds);

        for (int i=0; i < tournament.rounds; i++) {
            opening[i] = book[book.fetchId()];
        }

        CHECK(book_vector.size() == 10);
        CHECK(book_vector.capacity() == 10);
        CHECK(opening[0].fen == "rn1qkb1r/1p3p1p/p2p1np1/2pP4/4P1b1/2N2N2/PP2QPPP/R1B1KB1R w KQkq - 2 9");
        CHECK(opening[9].fen == "rnbqk2r/pp2p1bp/2pp1np1/5P2/3P4/2N3P1/PPP2PB1/R1BQK1NR w KQkq - 3 9");
    }
}

