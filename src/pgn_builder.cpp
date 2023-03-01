#include <iomanip> // std::setprecision
#include <sstream>

#include "pgn_builder.h"

PgnBuilder::PgnBuilder(const Match &match, const CMD::GameManagerOptions &gameOptions)
{
    const std::string result = resultToString(match.result);
    const std::string termination = match.termination;
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
    if (!termination.empty())
        ss << "[Termination " << "\"" << termination << "\"" << "]" << "\n";
    ss << "[TimeControl "   << "\"" << match.whiteEngine.tc     << "\"" << "]"  << "\n\n";
    // clang-format on

    std::stringstream illegalMove;

    if (!match.legal)
    {
        illegalMove << ", other side makes an illegal move: " << match.moves[match.moves.size() - 1].move;
    }

    Board b = match.board;

    int moveCount = 3;
    for (size_t i = 0; i < match.moves.size(); i++)
    {
        const MoveData data = match.moves[i];
        std::stringstream timeString;

        if (data.elapsedMillis >= 1000)
            timeString << std::fixed << std::setprecision(2) << data.elapsedMillis / 1000.0;
        else
            timeString << data.elapsedMillis;

        if (data.elapsedMillis >= 1000)
            timeString << "s";
        else
            timeString << "ms";

        std::stringstream nodesString;
        if (gameOptions.pgn.trackNodes)
        {
            nodesString << "/" << data.nodes;
        }

        if (moveCount % 2 != 0)
            ss << moveCount / 2 << "."
               << " " << MoveToSan(b, convertUciToMove(data.move)) << " {" << data.scoreString << "/" << data.depth
               << nodesString.str() << " " << timeString.str() << illegalMove.str() << "}";
        else
        {
            ss << " " << MoveToSan(b, convertUciToMove(data.move)) << " {" << data.scoreString << "/" << data.depth
               << nodesString.str() << " " << timeString.str() << illegalMove.str() << "}";
            if (i != match.moves.size() - 1 && i % 7 == 0)
                ss << "\n";
            else
                ss << " ";
        };

        if (!match.legal && i == match.moves.size() - 2)
        {
            break;
        }

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
