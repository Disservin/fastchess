#include <matchmaking/syzygy.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess;
using namespace chess;

TEST_SUITE("Syzygy Tests") {
    TEST_CASE("Should fail to load TBs") {
        const auto dirs    = "path_does_not_exist";
        const int tbPieces = initSyzygy(dirs);
        CHECK(tbPieces == 0);
    }

    TEST_CASE("Cannot probe if TBs not loaded") {
        // Kings and one pawn
        const auto board = Board::fromFen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
        CHECK(!canProbeSyzgyWdl(board));
    }
}