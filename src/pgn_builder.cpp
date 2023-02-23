#include <sstream>

#include "pgn_builder.h"

PgnBuilder::PgnBuilder(const std::vector<Match> &matches, CMD::GameManagerOptions gameOptions)
{
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
