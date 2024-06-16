#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
      options::Tournament tournament;
      tournament.rounds = 3000;
      tournament.file = "./data/openings.epd";
      tournament.seed = 123456789;
      tournament.opening.start = 3256;
      book::OpeningBook book(tournament);
      CHECK(book.book_[0] == "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9");
}

