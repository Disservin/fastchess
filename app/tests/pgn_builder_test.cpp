#include <pgn/pgn_builder.hpp>

#include "doctest/doctest.hpp"

namespace fastchess {
TEST_SUITE("PGN Builder Tests") {
    TEST_CASE("PGN Creation") {
        MatchData match_data;
        match_data.players.white.config.name = "engine1";
        match_data.players.white.color       = chess::Color::WHITE;
        match_data.players.white.result      = chess::GameResult::WIN;

        match_data.players.black.config.name = "engine2";
        match_data.players.black.color       = chess::Color::BLACK;
        match_data.players.black.result      = chess::GameResult::LOSE;

        match_data.moves = {MoveData("e2e4", "+1.00", 1321, 15, 4, 0, 0), MoveData("e7e5", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("g1f3", "+1.45", 310, 16, 24, 0, 0),
                            MoveData("g8f6", "+10.15", 1821, 18, 7, 0, 0)};

        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        match_data.termination = MatchTermination::NORMAL;

        match_data.reason = "engine2 got checkmated";

        config::Pgn pgn_config;
        pgn_config.site = "localhost";

        std::string expected = R"([Event "Fastchess Tournament"]
[Site "localhost"]
[Round "1"]
[White "engine1"]
[Black "engine2"]
[Result "1-0"]
[PlyCount "4"]
[Termination "normal"]
[TimeControl "-"]

1. e4 {+1.00/15 1.321s} e5 {+1.23/15 0.430s} 2. Nf3 {+1.45/16 0.310s}
Nf6 {+10.15/18 1.821s engine2 got checkmated} 1-0

)";

        pgn::PgnBuilder pgn_builder = pgn::PgnBuilder(pgn_config, match_data, 1);
        CHECK(pgn_builder.get() == expected);
    }

    TEST_CASE("PGN Creation Black Win") {
        MatchData match_data;
        match_data.players.white.config.name = "engine1";
        match_data.players.white.color       = chess::Color::WHITE;
        match_data.players.white.result      = chess::GameResult::LOSE;

        match_data.players.black.config.name = "engine2";
        match_data.players.black.color       = chess::Color::BLACK;
        match_data.players.black.result      = chess::GameResult::WIN;

        match_data.moves = {MoveData("e2e4", "+1.00", 1321, 15, 4, 0, 0), MoveData("e7e5", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("g1f3", "+1.45", 310, 16, 24, 0, 0),
                            MoveData("g8f6", "+10.15", 1821, 18, 7, 0, 0)};

        match_data.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        match_data.termination = MatchTermination::NORMAL;

        match_data.reason = "engine1 got checkmated";

        config::Pgn pgn_config;
        pgn_config.site = "localhost";

        std::string expected = R"([Event "Fastchess Tournament"]
[Site "localhost"]
[Round "1"]
[White "engine1"]
[Black "engine2"]
[Result "0-1"]
[PlyCount "4"]
[Termination "normal"]
[TimeControl "-"]

1. e4 {+1.00/15 1.321s} e5 {+1.23/15 0.430s} 2. Nf3 {+1.45/16 0.310s}
Nf6 {+10.15/18 1.821s engine1 got checkmated} 0-1

)";

        pgn::PgnBuilder pgn_builder = pgn::PgnBuilder(pgn_config, match_data, 1);
        CHECK(pgn_builder.get() == expected);
    }

    TEST_CASE("PGN Creation Black Start") {
        MatchData match_data;
        match_data.players.black.config.name = "engine1";
        match_data.players.black.color       = chess::Color::BLACK;
        match_data.players.black.result      = chess::GameResult::NONE;

        match_data.players.white.config.name = "engine2";
        match_data.players.white.color       = chess::Color::WHITE;
        match_data.players.white.result      = chess::GameResult::NONE;

        match_data.moves = {MoveData("e8g8", "+1.00", 1321, 15, 4, 0, 0), MoveData("e1g1", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("a6c5", "+1.45", 310, 16, 24, 0, 0)};

        match_data.fen = "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1";

        match_data.reason = "aborted";

        config::Pgn pgn_config;
        pgn_config.site = "localhost";

        std::string expected = R"([Event "Fastchess Tournament"]
[Site "localhost"]
[Round "1"]
[White "engine2"]
[Black "engine1"]
[Result "*"]
[SetUp "1"]
[FEN "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1"]
[PlyCount "3"]
[TimeControl "-"]

1... O-O {+1.00/15 1.321s} 2. O-O {+1.23/15 0.430s} Nc5 {+1.45/16 0.310s aborted} *

)";

        pgn::PgnBuilder pgn_builder = pgn::PgnBuilder(pgn_config, match_data, 1);
        CHECK(pgn_builder.get() == expected);
    }

    TEST_CASE("PGN Creation Fixed Time per Move") {
        MatchData match_data;
        match_data.players.black.config.name                = "engine1";
        match_data.players.black.color                      = chess::Color::BLACK;
        match_data.players.black.result                     = chess::GameResult::NONE;
        match_data.players.black.config.limit.tc.fixed_time = 1000;

        match_data.players.white.config.name                = "engine2";
        match_data.players.white.color                      = chess::Color::WHITE;
        match_data.players.white.result                     = chess::GameResult::NONE;
        match_data.players.white.config.limit.tc.fixed_time = 1000;

        match_data.moves = {MoveData("e8g8", "+1.00", 1321, 15, 4, 0, 0), MoveData("e1g1", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("a6c5", "+1.45", 310, 16, 24, 0, 0)};

        match_data.fen = "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1";

        match_data.reason = "aborted";

        config::Pgn pgn_config;
        pgn_config.site = "localhost";

        std::string expected = R"([Event "Fastchess Tournament"]
[Site "localhost"]
[Round "1"]
[White "engine2"]
[Black "engine1"]
[Result "*"]
[SetUp "1"]
[FEN "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1"]
[PlyCount "3"]
[TimeControl "1/move"]

1... O-O {+1.00/15 1.321s} 2. O-O {+1.23/15 0.430s} Nc5 {+1.45/16 0.310s aborted} *

)";

        pgn::PgnBuilder pgn_builder = pgn::PgnBuilder(pgn_config, match_data, 1);
        CHECK(pgn_builder.get() == expected);
    }

    TEST_CASE("PGN Creation TC") {
        MatchData match_data;
        match_data.players.black.config.name                = "engine1";
        match_data.players.black.color                      = chess::Color::BLACK;
        match_data.players.black.result                     = chess::GameResult::NONE;
        match_data.players.black.config.limit.tc.time       = 0;
        match_data.players.black.config.limit.tc.increment  = 5;

        match_data.players.white.config.name                = "engine2";
        match_data.players.white.color                      = chess::Color::WHITE;
        match_data.players.white.result                     = chess::GameResult::NONE;
        match_data.players.white.config.limit.tc.time       = 1;
        match_data.players.white.config.limit.tc.increment  = 5;

        match_data.moves = {MoveData("e8g8", "+1.00", 1321, 15, 4, 0, 0), MoveData("e1g1", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("a6c5", "+1.45", 310, 16, 24, 0, 0)};

        match_data.fen = "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1";

        match_data.reason = "aborted";

        config::Pgn pgn_config;
        pgn_config.site = "localhost";

        std::string expected = R"([Event "Fastchess Tournament"]
[Site "localhost"]
[Round "1"]
[White "engine2"]
[Black "engine1"]
[Result "*"]
[SetUp "1"]
[FEN "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1"]
[PlyCount "3"]
[WhiteTimeControl "0.001+0.005"]
[BlackTimeControl "0+0.005"]

1... O-O {+1.00/15 1.321s} 2. O-O {+1.23/15 0.430s} Nc5 {+1.45/16 0.310s aborted} *

)";

        pgn::PgnBuilder pgn_builder = pgn::PgnBuilder(pgn_config, match_data, 1);
        CHECK(pgn_builder.get() == expected);
    }

    TEST_CASE("PGN Creation Multiple Fixed Time per Move") {
        MatchData match_data;
        match_data.players.black.config.name                = "engine1";
        match_data.players.black.color                      = chess::Color::BLACK;
        match_data.players.black.result                     = chess::GameResult::NONE;
        match_data.players.black.config.limit.tc.fixed_time = 200;

        match_data.players.white.config.name                = "engine2";
        match_data.players.white.color                      = chess::Color::WHITE;
        match_data.players.white.result                     = chess::GameResult::NONE;
        match_data.players.white.config.limit.tc.fixed_time = 1000;

        match_data.moves = {MoveData("e8g8", "+1.00", 1321, 15, 4, 0, 0), MoveData("e1g1", "+1.23", 430, 15, 3, 0, 0),
                            MoveData("a6c5", "+1.45", 310, 16, 24, 0, 0)};

        match_data.fen = "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1";

        match_data.reason = "aborted";

        config::Pgn pgn_config;
        pgn_config.site = "localhost";

        std::string expected = R"([Event "Fastchess Tournament"]
[Site "localhost"]
[Round "1"]
[White "engine2"]
[Black "engine1"]
[Result "*"]
[SetUp "1"]
[FEN "r2qk2r/1bpp2pp/n3pn2/p2P1p2/1bP5/2N1BNP1/1PQ1PPBP/R3K2R b KQkq - 0 1"]
[PlyCount "3"]
[WhiteTimeControl "1/move"]
[BlackTimeControl "0.2/move"]

1... O-O {+1.00/15 1.321s} 2. O-O {+1.23/15 0.430s} Nc5 {+1.45/16 0.310s aborted} *

)";

        pgn::PgnBuilder pgn_builder = pgn::PgnBuilder(pgn_config, match_data, 1);
        CHECK(pgn_builder.get() == expected);
    }
}
}  // namespace fastchess
