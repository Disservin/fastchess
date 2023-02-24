#include <sstream>

#include "pgn_builder.h"

PgnBuilder::PgnBuilder(const Match &match, CMD::GameManagerOptions gameOptions)
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
        ss << "[Event "         << "\"" << gameOptions.eventName    << "\"" << "]" << "\n";
        ss << "[Site "          << "\"?\""                          << "]"  << "\n";
        ss << "[Date "          << match.date                       << "]"  << "\n";
        ss << "[Round "         << match.round                      << "]"  << "\n";
        ss << "[White "         << "\"" << match.whiteEngine.name    << "\"" << "]" << "\n";
        ss << "[Black "         << "\"" << match.blackEngine.name    << "\"" << "]" << "\n";
        ss << "[Result "        << "\"" << result                   << "\"" << "]" << "\n";
        ss << "[FEN "           << "\"" << match.board.getFen()     << "\"" << "]" << "\n";
        ss << "[GameDuration "  << "\"" << match.duration           << "\"" << "]" << "\n";
        ss << "[GameEndTime "   << "\"" << match.endTime            << "\"" << "]" << "\n";
        ss << "[GameStartTime " << "\"" << match.startTime          << "\"" << "]" << "\n";
        ss << "[PlyCount "      << "\"" << match.moves.size()       << "\"" << "]" << "\n";
        if (match.termination != "")
            ss << "[Termination " << "\"?\"" << "]" << "\n";
        ss << "[TimeControl "   << match.whiteEngine.tc             << "]"  << "\n\n";
    // clang-format on

    Board b = match.board;

    for (size_t i = 0; i < match.moves.size(); i++)
    {
        Move move = match.moves[i];
        if ((i + 3) % 2 != 0)
        {
            ss << (i + 3) / 2 << "."
               << " " << MoveToSan(b, move);
        }
        else
        {
            ss << " " << MoveToSan(b, move) << (i == match.moves.size() - 1 ? "\n" : "");
        }

        b.makeMove(move);

        i++;
    }
    ss << " " << result;

    pgn = ss.str();
}

std::string PgnBuilder::getPGN() const
{
    return pgn;
}
