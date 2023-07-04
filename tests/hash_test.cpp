#include "doctest/doctest.hpp"
#include "third_party/chess.hpp"

using namespace fast_chess;
using namespace chess;

TEST_SUITE("Zobrist Hash Tests") {
    TEST_CASE("Test Zobrist Hash Startpos") {
        Board b;

        b.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        CHECK(b.hash() == 0x463b96181691fc9c);
        CHECK(b.hash() == 0x463b96181691fc9c);

        b.makeMove(uci::uciToMove(b, "e2e4"));
        CHECK(b.hash() == 0x823c9b50fd114196);
        CHECK(b.hash() == 0x823c9b50fd114196);

        b.makeMove(uci::uciToMove(b, "d7d5"));
        CHECK(b.hash() == 0x0756b94461c50fb0);
        CHECK(b.hash() == 0x0756b94461c50fb0);

        b.makeMove(uci::uciToMove(b, "e4e5"));
        CHECK(b.hash() == 0x662fafb965db29d4);
        CHECK(b.hash() == 0x662fafb965db29d4);

        b.makeMove(uci::uciToMove(b, "f7f5"));
        CHECK(b.hash() == 0x22a48b5a8e47ff78);
        CHECK(b.hash() == 0x22a48b5a8e47ff78);

        b.makeMove(uci::uciToMove(b, "e1e2"));
        CHECK(b.hash() == 0x652a607ca3f242c1);
        CHECK(b.hash() == 0x652a607ca3f242c1);

        b.makeMove(uci::uciToMove(b, "e8f7"));
        CHECK(b.hash() == 0x00fdd303c946bdd9);
        CHECK(b.hash() == 0x00fdd303c946bdd9);
    }

    TEST_CASE("Test Zobrist Hash Second Position") {
        Board b;

        b.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        b.makeMove(uci::uciToMove(b, "a2a4"));
        b.makeMove(uci::uciToMove(b, "b7b5"));
        b.makeMove(uci::uciToMove(b, "h2h4"));
        b.makeMove(uci::uciToMove(b, "b5b4"));
        b.makeMove(uci::uciToMove(b, "c2c4"));
        CHECK(b.hash() == 0x3c8123ea7b067637);

        b.makeMove(uci::uciToMove(b, "b4c3"));
        b.makeMove(uci::uciToMove(b, "a1a3"));
        CHECK(b.hash() == 0x5c3f9b829b279560);
    }
}