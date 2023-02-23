#include <sstream>

#include "pgn_builder.h"

PgnBuilder::PgnBuilder(const std::vector<Match> &matches, CMD::GameManagerOptions gameOptions)
{

    // [Event "?"]
    // [Site "?"]
    // [Date "2023.01.24"]
    // [Round "1"]
    // [White ".\smallbrain.exe"]
    // [Black ".\Caissa_1.5_AVX2_BMI2.exe"]
    // [Result "*"]
    // [FEN "r1b1kbnr/1p3ppp/p2p4/q3p3/2PQP3/P1N5/1P3PPP/R1B1KB1R w KQkq - 0 1"]
    // [GameDuration "00:00:10"]
    // [GameEndTime "2023-01-24T23:06:07.769 Mitteleuropäische Zeit"]
    // [GameStartTime "2023-01-24T23:05:57.204 Mitteleuropäische Zeit"]
    // [PlyCount "37"]
    // [SetUp "1"]
    // [Termination "unterminated"]
    // [TimeControl "8+0.08"]

    for (auto match : matches)
    {
        std::string result;
        if (match.result == GameResult::BLACK_WIN)
        {
            result = "0-1";
        }
        else if (match.result == GameResult::WHITE_WIN)
        {
            result = "1-0";
        }
        else if (match.result == GameResult::DRAW)
        {
            result = "1/2-1/2";
        }
        else
        {
            result = "*";
        }

        std::stringstream ss;

        // clang-format off
        ss << "[Event " << "\""<< gameOptions.eventName <<"\"" << "]" << "\n";
        ss << "[Site " << "\"?\"" << "]" << "\n";
        ss << "[Date " << match.date << "]" << "\n";
        ss << "[Round " << match.round << "]" << "\n";
        ss << "[White " << "\""<<match.whiteEngine.cmd<<"\"" << "]" << "\n";
        ss << "[Black " << "\""<<match.blackEngine.cmd<<"\"" << "]" << "\n";
        ss << "[Result " << "\"" << result << "\"" << "]" << "\n";
        ss << "[FEN " << "\"" << match.board.getFen() << "\""<< "]" << "\n";
        ss << "[GameDuration " << "\"" << match.duration << "\"" << "]" << "\n";
        ss << "[GameEndTime " << "\"" << match.endTime << "\"" << "]" << "\n";
        ss << "[GameStartTime " << "\"" << match.startTime << "\"" << "]" << "\n";
        ss << "[PlyCount " << "\"" << match.moves.size() <<"\"" << "]" << "\n";
        // ss << "[Termination " << "\"?\"" << "]" << "\n";
        ss << "[TimeControl " << match.whiteEngine.tc << "]" << "\n\n";
        // clang-format on

        Board b = match.board;

        int i = 3;
        for (auto move : match.moves)
        {
            if (i % 2 != 0)
            {
                ss << i / 2 << "."
                   << " " << MoveToSan(b, move);
            }
            else
            {
                ss << " " << MoveToSan(b, move) << (move != match.moves.back() ? "\n" : "");
            }
            b.makeMove(move);

            i++;
        }
        ss << " " << result;
        std::cout << ss.str() << std::endl;
    }
}
