#include <sstream>

#include "pgn_builder.h"

PgnBuilder::PgnBuilder(const Match &match, CMD::GameManagerOptions gameOptions)
{
    std::string result = resultToString(match.result);
    std::string termination = match.termination;
    std::stringstream ss;

    if (!match.legal)
    {
        termination = std::string("illegal move");
    }

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
    if (termination != "")
        ss << "[Termination " << "\"" << termination << "\"" << "]" << "\n";
    ss << "[TimeControl "   << "\"" << match.whiteEngine.tc     << "\"" << "]"  << "\n\n";
    // clang-format on

    Board b = match.board;

    int moveCount = 3;
    for (size_t i = 0; i < match.moves.size(); i++)
    {
        MoveData data = match.moves[i];

        if (!match.legal && i == match.moves.size() - 2)
        {
            ss << " " << data.move << " {" << data.scoreString << "/" << data.depth << " " << data.elapsedMillis
               << "ms, " << (~b.sideToMove == WHITE ? "White" : "Black")
               << " makes an illegal move: " << match.moves[i + 1].move;
            break;
        }

        if (moveCount % 2 != 0)
            ss << moveCount / 2 << "."
               << " " << data.move << " {" << data.scoreString << "/" << data.depth << " " << data.elapsedMillis
               << "ms}";
        else
        {
            ss << " " << data.move << " {" << data.scoreString << "/" << data.depth << " " << data.elapsedMillis
               << "ms}";
        };

        if (i != match.moves.size() - 1 && i % 7 == 0)
            ss << "\n";
        else
            ss << " ";

        b.makeMove(convertUciToMove(data.move));

        moveCount++;
    }

    ss << " " << result << "\n";

    pgn = ss.str();
}

std::string PgnBuilder::getPGN() const
{
    return pgn;
}
