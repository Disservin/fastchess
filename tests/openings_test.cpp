#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
      options::Tournament tournament;
      tournament.rounds = 3000;
      tournament.opening.file = "./data/openings.epd";
      tournament.seed = 123456789;
      tournament.opening.start = 3256;
      book::OpeningBook book(tournament);
      book.book_[0] = 
}

