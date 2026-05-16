#include <game/epd/epd_builder.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

namespace {

MoveData makeMoveData(std::string move, Score score, int64_t elapsed_millis, int64_t depth, int64_t seldepth,
                      uint64_t nodes, bool legal = true) {
    MoveData move_data(std::move(move), elapsed_millis, depth, seldepth, nodes, legal);
    move_data.score = score;
    return move_data;
}

}  // namespace

TEST_SUITE("EPD Builder Tests") {
    TEST_CASE("EPD Creation White Start") {
        MatchData match_data;

        match_data.moves = {makeMoveData("e2e4", {ScoreType::CP, 100}, 1321, 15, 4, 0),
                            makeMoveData("e7e5", {ScoreType::CP, 123}, 430, 15, 3, 0),
                            makeMoveData("g1f3", {ScoreType::CP, 145}, 310, 16, 24, 0),
                            makeMoveData("g8f6", {ScoreType::CP, 1015}, 1821, 18, 7, 0)};

        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        std::string expected = "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - hmvc 2; fmvn 3;\n";

        epd::EpdBuilder epd_builder = epd::EpdBuilder(VariantType::STANDARD, match_data);
        CHECK(epd_builder.get() == expected);
    }

    TEST_CASE("EPD Creation Black Start") {
        MatchData match_data;

        match_data.moves = {makeMoveData("e8g8", {ScoreType::CP, 100}, 1321, 15, 4, 0),
                            makeMoveData("e1g1", {ScoreType::CP, 123}, 430, 15, 3, 0),
                            makeMoveData("a6c5", {ScoreType::CP, 145}, 310, 16, 24, 0)};

        match_data.fen = "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1";

        std::string expected = "r2q1rk1/1bpp2pp/4pn2/p1nP1p2/1bP5/2N1BNP1/1PQ1PPBP/R4RK1 w - - hmvc 3; fmvn 3;\n";

        epd::EpdBuilder epd_builder = epd::EpdBuilder(VariantType::STANDARD, match_data);
        CHECK(epd_builder.get() == expected);
    }
}
}  // namespace fastchess
