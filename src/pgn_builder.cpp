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
        ss << "[Date "          << "\"" << match.date               << "\"" << "]"  << "\n";
        ss << "[Round "         << "\"" << match.round              << "\"" << "]"  << "\n";
        ss << "[White "         << "\"" << match.whiteEngine.name   << "\"" << "]" << "\n";
        ss << "[Black "         << "\"" << match.blackEngine.name   << "\"" << "]" << "\n";
        ss << "[Result "        << "\"" << result                   << "\"" << "]" << "\n";
        ss << "[FEN "           << "\"" << match.board.getFen()     << "\"" << "]" << "\n";
        ss << "[GameDuration "  << "\"" << match.duration           << "\"" << "]" << "\n";
        ss << "[GameEndTime "   << "\"" << match.endTime            << "\"" << "]" << "\n";
        ss << "[GameStartTime " << "\"" << match.startTime          << "\"" << "]" << "\n";
        ss << "[PlyCount "      << "\"" << match.moves.size()       << "\"" << "]" << "\n";
        if (match.termination != "")
            ss << "[Termination " << "\"?\"" << "]" << "\n";
        ss << "[TimeControl "   << "\"" << match.whiteEngine.tc     << "\"" << "]"  << "\n\n";
    // clang-format on

    Board b = match.board;

    int moveCount = 3;
    for (size_t i = 0; i < match.moves.size(); i++)
    {
        MoveData data = match.moves[i];

        if (moveCount % 2 != 0)
            ss << moveCount / 2 << "."
               << " " << MoveToSan(b, data.move) << " {" << data.scoreString << "/" << data.depth << " " << data.elapsedMillis << "ms}";
        else
        {
            ss << " " << MoveToSan(b, data.move) << " {" << data.scoreString << "/" << data.depth << " " << data.elapsedMillis << "ms}";
            if (i != match.moves.size() - 1 && i % 7 == 0)
                ss << "\n";
            else
                ss << " ";
        }

        b.makeMove(data.move);

        moveCount++;
    }

    ss << " " << result << "\n";

    pgn = ss.str();
}

std::string PgnBuilder::getPGN() const
{
    return pgn;
}
