#pragma once

#include "../src/board.h"

inline bool testFenRepetition(const std::string &input)
{
    Board board;

    std::vector<std::string> tokens = splitString(input, ' ');

    bool hasMoves = contains(tokens, "moves");

    if (tokens[1] == "fen")
        board.loadFen(input.substr(input.find("fen") + 4));
    else
        board.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    if (hasMoves)
    {
        std::size_t index = std::find(tokens.begin(), tokens.end(), "moves") - tokens.begin();
        index++;
        for (; index < tokens.size(); index++)
        {
            Move move = convertUciToMove(tokens[index]);
            board.makeMove(move);
        }
    }

    return board.isRepetition(2);
}

TEST_CASE("Test First Repetition")
{
    std::string input =
        "position fen r2qk2r/pp3pp1/2nbpn1p/8/6b1/2NP1N2/PPP1BPPP/R1BQ1RK1 w kq - 0 1 moves h2h3 g4f5 d3d4 e8g8 e2d3 "
        "f5d3 d1d3 a8c8 a2a3 f8e8 f1e1 e6e5 d4e5 c6e5 f3e5 d6e5 d3d8 c8d8 c3d1 f6e4 d1e3 e5d4 g1f1 d4c5 e1d1 d8d1 e3d1 "
        "c5f2 d1f2 e4g3 f1g1 e8e1 g1h2 g3f1 h2g1 f1g3 g1h2 g3f1 h2g1 f1g3 ";

    CHECK(testFenRepetition(input));
}

TEST_CASE("Test Second Repetition")
{
    std::string input =
        "position fen 6Q1/1p1k3p/3b2p1/p6r/3P1PR1/1PR2NK1/P3q2P/4r3 b - - 14 37 moves e1f1 f3e5 d6e5 g8f7 d7d8 "
        "f7f8 d8d7 f8f7 d7d8 f7f8 d8d7 f8f7";

    CHECK(testFenRepetition(input));
}

TEST_CASE("Test Third Repetition")
{
    std::string input =
        "position fen rn2k1nr/pp2bppp/2p5/q3p3/2B5/P1N2b1P/1PPP1PP1/R1BQ1RK1 w kq - 0 1 moves d1f3 g8f6 c3e2 e8g8 "
        "e2g3 b8d7 d2d3 a5c7 g3f5 e7d8 c4a2 a7a5 f3g3 f6h5 g3f3 h5f6 f3g3 f6h5 g3f3 h5f6";

    CHECK(testFenRepetition(input));
}

TEST_CASE("Test Fourth Repetition")
{
    std::string input =
        "position fen rnbqk2r/1pp2ppp/p2bpn2/8/2NP1B2/3B1N2/PPP2PPP/R2QK2R w KQkq - 0 1 moves c4d6 c7d6 c2c4 e8g8 "
        "e1g1 b7b6 f1e1 c8b7 f4g3 f8e8 a1c1 h7h6 d3b1 b6b5 b2b3 b5c4 b3c4 d8b6 c4c5 d6c5 d4c5 b6c6 b1c2 c6d5 g3d6 "
        "e8c8 c2b3 d5d1 e1d1 a6a5 f3d4 a5a4 b3c4 b7d5 a2a3 b8c6 c4d5 f6d5 d4c6 c8c6 c1c4 d5c7 d1b1 c7e8 d6f4 a8c8 "
        "b1b8 c8b8 f4b8 f7f6 g1f1 g8f7 f2f4 g7g5 g2g3 f7g6 f1e2 c6c8 b8a7 e8c7 e2d3 c8d8 d3e4 f6f5 e4e3 d8a8 a7b6 "
        "c7d5 e3d4 a8a6 b6d8 a6a8 d8b6 a8a6 b6d8 a6a8 d8b6";

    CHECK(testFenRepetition(input));
}