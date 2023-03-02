#pragma once

#include "../src/chess/board.hpp"

TEST_CASE("Test ambiguous pawn capture")
{
    Board b;
    b.loadFen("rnbqkbnr/ppp1p1pp/3p1p2/4N3/8/3P4/PPPKPPPP/R1BQ1BNR b kq - 1 7");

    Move m = {SQ_F6, SQ_E5, PAWN, NONETYPE};

    CHECK(MoveToSan(b, m) == "fxe5");
}

TEST_CASE("Test ambiguous pawn ep capture")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppp1p/8/5PpP/8/8/PPPPP2P/RNBQKBNR w KQkq g6 0 2");

    Move m = {SQ_F5, SQ_G6, PAWN, NONETYPE};

    CHECK(MoveToSan(b, m) == "fxg6");
}

TEST_CASE("Test ambiguous knight move")
{
    Board b;
    b.loadFen("rnbqkbnr/ppp3pp/3ppp2/8/8/3P1N1N/PPPKPPPP/R1BQ1B1R w kq - 1 8");

    Move m = {SQ_F3, SQ_G5, KNIGHT, NONETYPE};

    CHECK(MoveToSan(b, m) == "Nfg5");
}

TEST_CASE("Test ambiguous rook move with check")
{
    Board b;
    b.loadFen("4k3/8/8/8/8/8/2R3R1/2K5 w - - 0 1");

    Move m = {SQ_C2, SQ_E2, ROOK, NONETYPE};

    CHECK(MoveToSan(b, m) == "Rce2+");
}

TEST_CASE("Test ambiguous rook move with checkmate")
{
    Board b;
    b.loadFen("7k/8/8/8/8/8/2K3R1/3R4 w - - 0 1");

    Move m = {SQ_D1, SQ_H1, ROOK, NONETYPE};

    CHECK(MoveToSan(b, m) == "Rh1#");
}

TEST_CASE("Test Pawn move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    Move m = {SQ_E2, SQ_E4, PAWN, NONETYPE};

    CHECK(MoveToSan(b, m) == "e4");
}

TEST_CASE("Test Knight move")
{
    Board b;
    b.loadFen("rnbqkbnr/ppp1p1pp/3p1p2/8/8/3P1N2/PPPKPPPP/R1BQ1BNR w kq - 0 7");

    Move m = {SQ_F3, SQ_G5, KNIGHT, NONETYPE};

    CHECK(MoveToSan(b, m) == "Ng5");
}

TEST_CASE("Test Bishop move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");

    Move m = {SQ_E1, SQ_F1, BISHOP, NONETYPE};

    CHECK(MoveToSan(b, m) == "Kf1");
}

TEST_CASE("Test Rook move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPP1PP/R3KR2 w Qkq - 0 1");

    Move m = {SQ_F1, SQ_F7, ROOK, NONETYPE};

    CHECK(MoveToSan(b, m) == "Rxf7");
}

TEST_CASE("Test Queen move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPP1PP/R3KQ1R w KQkq - 0 1");

    Move m = {SQ_F1, SQ_F7, QUEEN, NONETYPE};

    CHECK(MoveToSan(b, m) == "Qxf7+");
}

TEST_CASE("Test King move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");

    Move m = {SQ_E1, SQ_F1, KING, NONETYPE};

    CHECK(MoveToSan(b, m) == "Kf1");
}

TEST_CASE("Test King Castling Short move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 17");

    Move m = {SQ_E1, SQ_G1, KING, NONETYPE};

    CHECK(MoveToSan(b, m) == "O-O");
}

TEST_CASE("Test King Castling Short move")
{
    Board b;
    b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");

    Move m = {SQ_E1, SQ_C1, KING, NONETYPE};

    CHECK(MoveToSan(b, m) == "O-O-O");
}

TEST_CASE("Test Pawn Promotion")
{
    Board b;
    b.loadFen("2k5/5P2/8/8/8/4K3/8/8 w - - 0 1");

    Move m = {SQ_F7, SQ_F8, PAWN, QUEEN};

    CHECK(MoveToSan(b, m) == "f8=Q+");
}

TEST_CASE("Test Pawn Promotion")
{
    Board b;
    b.loadFen("3k2n1/5P1P/8/8/8/8/8/4K3 w - - 0 1");

    Move m = {SQ_F7, SQ_G8, PAWN, QUEEN};

    CHECK(MoveToSan(b, m) == "fxg8=Q+");
}