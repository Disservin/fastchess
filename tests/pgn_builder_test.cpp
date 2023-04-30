#include <pgn_builder.hpp>

#include <cassert>
#include <chrono>
#include <thread>

#include "doctest/doctest.hpp"

namespace fast_chess {
TEST_SUITE("PGN Builder Tests") {
    TEST_CASE("PGN Creation") {
        MatchData match_data;
        match_data.players.first.config.name = "engine1";
        match_data.players.first.color = Chess::Color::WHITE;
        match_data.players.first.result = Chess::GameResult::WIN;

        match_data.players.second.config.name = "engine2";
        match_data.players.second.color = Chess::Color::BLACK;
        match_data.players.second.result = Chess::GameResult::LOSE;

        match_data.moves = {MoveData("e2e4", "+1.00", 1321, 15, 4, 0, 0),
                            MoveData("e7e5", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("g1f3", "+1.45", 310, 16, 24, 0, 0),
                            MoveData("g8f6", "+10.15", 1821, 18, 7, 0, 0)};

        match_data.termination = "";
        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        match_data.internal_reason = "checkmate";

        match_data.round = 1;

        CMD::GameManagerOptions options;
        options.site = "localhost";

        PgnBuilder pgn_builder = PgnBuilder(match_data, options);
        CHECK(pgn_builder.get() ==
              "[Event \"Fast Chess\"]\n[Site \"localhost\"]\n[Round \"1\"]\n[White "
              "\"engine1\"]\n[Black \"engine2\"]\n[Result \"1-0\"]\n[FEN "
              "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n[PlyCount "
              "\"4\"]\n[TimeControl \"0\"]\n\n1. e4 {+1.00/15, 1.321s} e5 {+1.23/15, 0.430s}2. Nf3 "
              "{+1.45/16, 0.310s}\n Nf6 {+10.15/18, 1.821s}\n\n");
    }

    TEST_CASE("PGN Creation Black Win") {
        MatchData match_data;
        match_data.players.first.config.name = "engine1";
        match_data.players.first.color = Chess::Color::WHITE;
        match_data.players.first.result = Chess::GameResult::LOSE;

        match_data.players.second.config.name = "engine2";
        match_data.players.second.color = Chess::Color::BLACK;
        match_data.players.second.result = Chess::GameResult::WIN;

        match_data.moves = {MoveData("e2e4", "+1.00", 1321, 15, 4, 0, 0),
                            MoveData("e7e5", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("g1f3", "+1.45", 310, 16, 24, 0, 0),
                            MoveData("g8f6", "+10.15", 1821, 18, 7, 0, 0)};

        match_data.termination = "";
        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        match_data.internal_reason = "checkmate";

        match_data.round = 1;

        CMD::GameManagerOptions options;
        options.site = "localhost";

        PgnBuilder pgn_builder = PgnBuilder(match_data, options);
        CHECK(pgn_builder.get() ==
              "[Event \"Fast Chess\"]\n[Site \"localhost\"]\n[Round \"1\"]\n[White "
              "\"engine1\"]\n[Black \"engine2\"]\n[Result \"0-1\"]\n[FEN "
              "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n[PlyCount "
              "\"4\"]\n[TimeControl \"0\"]\n\n1. e4 {+1.00/15, 1.321s} e5 {+1.23/15, 0.430s}2. Nf3 "
              "{+1.45/16, 0.310s}\n Nf6 {+10.15/18, 1.821s}\n\n");
    }
}
}  // namespace fast_chess
