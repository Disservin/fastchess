#include <matchmaking/syzygy.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess;
using namespace chess;

namespace {
const std::string syzygy3MenPath = "./app/tests/data/syzygy_wdl3";
const std::string syzygy4MenPath = "./app/tests/data/syzygy_wdl4";

// The separator used by Pyrrhic depends on the OS.
#ifdef _WIN32
const std::string separator = ";";
#else
const std::string separator = ":";
#endif
}  // namespace

TEST_SUITE("Syzygy Tests") {
    TEST_CASE("Should fail to load non-existent TBs") {
        const auto dirs    = "path_does_not_exist";
        const int tbPieces = initSyzygy(dirs);
        CHECK(tbPieces == 0);
    }

    TEST_CASE("Cannot probe if TBs not loaded") {
        // Kings and one white pawn.
        const auto board = Board::fromFen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
        CHECK(!canProbeSyzgyWdl(board));
    }

    TEST_CASE("Can use 4-men TBs") {
        const auto syzygyDirs = syzygy3MenPath + separator + syzygy4MenPath;
        const int tbPieces    = initSyzygy(syzygyDirs);

        // Kings and one pawn each.
        const auto onePawnEach = Board::fromFen("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");

        try {
            CHECK(tbPieces == 4);

            CHECK(canProbeSyzgyWdl(onePawnEach));
            CHECK(probeSyzygyWdl(onePawnEach) == GameResult::DRAW);

            // Kings and one white pawn.
            const auto oneWhitePawn = Board::fromFen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
            CHECK(canProbeSyzgyWdl(oneWhitePawn));
            // If white is to move, white wins.
            CHECK(probeSyzygyWdl(oneWhitePawn) == GameResult::WIN);

            // Kings and one black pawn.
            const auto oneBlackPawn = Board::fromFen("4k3/4p3/8/8/8/8/8/4K3 b - - 0 1");
            CHECK(canProbeSyzgyWdl(oneBlackPawn));
            // If black is to move, black wins.
            CHECK(probeSyzygyWdl(oneBlackPawn) == GameResult::WIN);

            // Kings and one black queen.
            const auto oneBlackQueen = Board::fromFen("4k3/4q3/8/8/8/8/8/4K3 w - - 0 1");
            CHECK(canProbeSyzgyWdl(oneBlackQueen));
            // Black wins, with white to move.
            CHECK(probeSyzygyWdl(oneBlackQueen) == GameResult::LOSE);

            // Half move clock 2, cannot probe.
            const auto nonZeroHalfMove = Board::fromFen("4k3/4p3/8/8/8/8/4P3/4K3 w - - 2 2");
            CHECK(!canProbeSyzgyWdl(nonZeroHalfMove));

            // Castling rights, cannot probe.
            const auto castlingRights = Board::fromFen("4k3/8/8/8/8/8/8/4K2R w K - 0 1");
            CHECK(!canProbeSyzgyWdl(castlingRights));

            // 5 men, cannot probe.
            const auto fiveMen = Board::fromFen("4k3/4p3/8/8/8/8/4PP2/4K3 w - - 0 1");
            CHECK(!canProbeSyzgyWdl(fiveMen));

            tearDownSyzygy();
        } catch (...) {
            tearDownSyzygy();
            throw;
        }

        // Can no longer probe
        CHECK(!canProbeSyzgyWdl(onePawnEach));
    }
}
