#include <book/opening_book.hpp>

#include "doctest/doctest.hpp"

using namespace fast_chess;

TEST_SUITE("Openings") {
      options::Tournament tournament;
      
      book::OpeningBook book(tournament);
}

