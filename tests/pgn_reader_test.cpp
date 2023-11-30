#include <pgn/pgn_reader.hpp>

#include <chess.hpp>
#include "doctest/doctest.hpp"

namespace fast_chess {
TEST_SUITE("PGN Reader") {
    TEST_CASE("Read PGN file") {
        PgnReader reader("tests/data/test.pgn");
        const auto games = reader.getPgns();

        CHECK(games.size() == 6);

        CHECK(games[0].fen == chess::constants::STARTPOS);
        CHECK(games[1].fen == chess::constants::STARTPOS);
        CHECK(games[2].fen == chess::constants::STARTPOS);
        CHECK(games[3].fen == "5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34");
        CHECK(games[4].fen == "5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59");
        CHECK(games[5].fen == "8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28");

        CHECK(games[0].moves.size() == 16);
        CHECK(games[1].moves.size() == 16);
        CHECK(games[2].moves.size() == 16);
        CHECK(games[3].moves.size() == 0);
        CHECK(games[4].moves.size() == 0);
        CHECK(games[5].moves.size() == 0);

        auto convert = [](const std::vector<std::string> &moves) {
            chess::Board board;
            std::vector<std::string> uci_moves;
            for (const auto& move : moves) {
                chess::Move i_move = chess::uci::parseSan(board, move);

                uci_moves.push_back(chess::uci::moveToUci(i_move, board.chess960()));
                board.makeMove(i_move);
            }
            return uci_moves;
        };

        // First game
        {
            chess::Board board = chess::Board();

            const std::vector<std::string> moves = {"e4",   "e5", "Nf3",  "Nc6", "Bc4", "Nf6",
                                                    "Ng5",  "d5", "exd5", "Na5", "d3",  "Nxc4",
                                                    "dxc4", "h6", "Nf3",  "e4"};

            const auto uci_moves = convert(moves);

            const std::vector<std::string> correct = {
                "e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6", "f3g5", "d7d5",
                "e4d5", "c6a5", "d2d3", "a5c4", "d3c4", "h7h6", "g5f3", "e5e4"};

            for (size_t i = 0; i < moves.size(); i++) {
                const auto move = chess::uci::parseSan(board, moves[i]);
                CHECK(games[0].moves[i] == move);
                board.makeMove(move);
            }

            for (size_t i = 0; i < correct.size(); i++) {
                CHECK(chess::uci::moveToUci(games[0].moves[i], false) == correct[i]);
            }
        }

        // Second game
        {
            chess::Board board = chess::Board();

            const std::vector<std::string> moves = {"c4",   "c5",   "Nf3",  "Nc6", "d4",  "cxd4",
                                                    "Nxd4", "Nxd4", "Qxd4", "d6",  "Nc3", "e5",
                                                    "Qd3",  "Be7",  "g3",   "h6"};

            const auto uci_moves = convert(moves);

            const std::vector<std::string> correct = {
                "c2c4", "c7c5", "g1f3", "b8c6", "d2d4", "c5d4", "f3d4", "c6d4",
                "d1d4", "d7d6", "b1c3", "e7e5", "d4d3", "f8e7", "g2g3", "h7h6"};

            std::cout << "fen: " << board.getFen() << "\n";

            for (size_t i = 0; i < moves.size(); i++) {
                const auto move = chess::uci::parseSan(board, moves[i]);
                CHECK(games[1].moves[i] == move);
                board.makeMove(move);
            }

            for (size_t i = 0; i < correct.size(); i++) {
                CHECK(chess::uci::moveToUci(games[1].moves[i], false) == correct[i]);
            }
        }

        // Third game
        {
            chess::Board board = chess::Board();

            const std::vector<std::string> moves = {"e4",  "e6",  "d4",   "d5",   "Nc3", "Nf6",
                                                    "Bg5", "Be7", "e5",   "Nfd7", "h4",  "f6",
                                                    "Bd3", "c5",  "Qh5+", "Kf8"};

            const auto uci_moves = convert(moves);

            const std::vector<std::string> correct = {
                "e2e4", "e7e6", "d2d4", "d7d5", "b1c3", "g8f6", "c1g5", "f8e7",
                "e4e5", "f6d7", "h2h4", "f7f6", "f1d3", "c7c5", "d1h5", "e8f8"};

            for (size_t i = 0; i < moves.size(); i++) {
                const auto move = chess::uci::parseSan(board, moves[i]);
                CHECK(games[2].moves[i] == move);
                board.makeMove(move);
            }

            for (size_t i = 0; i < correct.size(); i++) {
                CHECK(chess::uci::moveToUci(games[2].moves[i], false) == correct[i]);
            }
        }
    }
}
}  // namespace fast_chess
