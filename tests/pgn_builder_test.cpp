#include <pgn/pgn_builder.hpp>

#include "doctest/doctest.hpp"

namespace fast_chess {
TEST_SUITE("PGN Builder Tests") {
    TEST_CASE("PGN Creation") {
        MatchData match_data;
        match_data.players.first.config.name = "engine1";
        match_data.players.first.color       = chess::Color::WHITE;
        match_data.players.first.result      = chess::GameResult::WIN;

        match_data.players.second.config.name = "engine2";
        match_data.players.second.color       = chess::Color::BLACK;
        match_data.players.second.result      = chess::GameResult::LOSE;

        match_data.moves = {MoveData("e2e4", "+1.00", 1321, 15, 4, 0, 0),
                            MoveData("e7e5", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("g1f3", "+1.45", 310, 16, 24, 0, 0),
                            MoveData("g8f6", "+10.15", 1821, 18, 7, 0, 0)};

        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        match_data.reason = "engine2 got checkmated";

        options::Tournament options;
        options.site = "localhost";

        PgnBuilder pgn_builder = PgnBuilder(match_data, options, 1);
        CHECK(
            pgn_builder.get() ==
            "[Event \"Fast Chess\"]\n[Site \"localhost\"]\n[Round \"1\"]\n[White "
            "\"engine1\"]\n[Black \"engine2\"]\n[Result \"1-0\"]\n[FEN "
            "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n[PlyCount "
            "\"4\"]\n[TimeControl \"0\"]\n\n1. e4 {+1.00/15, 1.321s} e5 {+1.23/15, 0.430s} 2. Nf3 "
            "{+1.45/16, 0.310s}\nNf6 {+10.15/18, 1.821s, engine2 got checkmated} 1-0\n\n");
    }

    TEST_CASE("PGN Creation Black Win") {
        MatchData match_data;
        match_data.players.first.config.name = "engine1";
        match_data.players.first.color       = chess::Color::WHITE;
        match_data.players.first.result      = chess::GameResult::LOSE;

        match_data.players.second.config.name = "engine2";
        match_data.players.second.color       = chess::Color::BLACK;
        match_data.players.second.result      = chess::GameResult::WIN;

        match_data.moves = {MoveData("e2e4", "+1.00", 1321, 15, 4, 0, 0),
                            MoveData("e7e5", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("g1f3", "+1.45", 310, 16, 24, 0, 0),
                            MoveData("g8f6", "+10.15", 1821, 18, 7, 0, 0)};

        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        match_data.reason = "engine1 got checkmated";

        options::Tournament options;
        options.site = "localhost";

        PgnBuilder pgn_builder = PgnBuilder(match_data, options, 1);
        CHECK(
            pgn_builder.get() ==
            "[Event \"Fast Chess\"]\n[Site \"localhost\"]\n[Round \"1\"]\n[White "
            "\"engine1\"]\n[Black \"engine2\"]\n[Result \"0-1\"]\n[FEN "
            "\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n[PlyCount "
            "\"4\"]\n[TimeControl \"0\"]\n\n1. e4 {+1.00/15, 1.321s} e5 {+1.23/15, 0.430s} 2. Nf3 "
            "{+1.45/16, 0.310s}\nNf6 {+10.15/18, 1.821s, engine1 got checkmated} 0-1\n\n");
    }

    TEST_CASE("PGN Creation Black Start") {
        MatchData match_data;
        match_data.players.first.config.name = "engine1";
        match_data.players.first.color       = chess::Color::BLACK;
        match_data.players.first.result      = chess::GameResult::NONE;

        match_data.players.second.config.name = "engine2";
        match_data.players.second.color       = chess::Color::WHITE;
        match_data.players.second.result      = chess::GameResult::NONE;

        match_data.moves = {MoveData("e8g8", "+1.00", 1321, 15, 4, 0, 0),
                            MoveData("e1g1", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("a6c5", "+1.45", 310, 16, 24, 0, 0)};

        match_data.fen = "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1";

        match_data.reason = "aborted";

        options::Tournament options;
        options.site = "localhost";

        PgnBuilder pgn_builder = PgnBuilder(match_data, options, 1);
        CHECK(
            pgn_builder.get() ==
            "[Event \"Fast Chess\"]\n[Site \"localhost\"]\n[Round \"1\"]\n[White "
            "\"engine2\"]\n[Black \"engine1\"]\n[Result \"*\"]\n[SetUp \"1\"]\n[FEN "
            "\"r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1\"]\n[PlyCount "
            "\"3\"]\n[TimeControl \"0\"]\n\n1... O-O {+1.00/15, 1.321s} 2. O-O {+1.23/15, "
            "0.430s}\nNc5 {+1.45/16, 0.310s, aborted} *\n\n");
    }
}
}  // namespace fast_chess
