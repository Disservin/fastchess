#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
      fast_chess::options::Tournament.rounds = 3000;
      fast_chess::options::Tournament.file = "./data/openings.epd";
      fast_chess::options::Tournament.seed = 123456789;
      fast_chess::options::Tournament.opening.start = 3256;
      book::OpeningBook book(tournament);
      CHECK(book.book_[0] == "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9");
}

