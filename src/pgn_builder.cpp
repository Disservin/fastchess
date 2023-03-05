#include <iomanip> // std::setprecision
#include <sstream>

#include "pgn_builder.hpp"

PgnBuilder::PgnBuilder(const Match &match, const CMD::GameManagerOptions &gameOptions)
{
    const std::string result = resultToString(match.result);
    const std::string termination = match.termination;
    std::stringstream ss;

    // clang-format off
    ss << "[Event "         << "\"" << gameOptions.eventName    << "\"" << "]" << "\n";
    ss << "[Site "          << "\"" << gameOptions.site         << "\"" << "]" << "\n";
    ss << "[Date "          << "\"" << match.date               << "\"" << "]" << "\n";
    ss << "[Round "         << "\"" << match.round              << "\"" << "]" << "\n";
    ss << "[White "         << "\"" << match.whiteEngine.name   << "\"" << "]" << "\n";
    ss << "[Black "         << "\"" << match.blackEngine.name   << "\"" << "]" << "\n";
    ss << "[Result "        << "\"" << result                   << "\"" << "]" << "\n";
    ss << "[FEN "           << "\"" << match.fen                << "\"" << "]" << "\n";
    ss << "[GameDuration "  << "\"" << match.duration           << "\"" << "]" << "\n";
    ss << "[GameEndTime "   << "\"" << match.endTime            << "\"" << "]" << "\n";
    ss << "[GameStartTime " << "\"" << match.startTime          << "\"" << "]" << "\n";
    ss << "[PlyCount "      << "\"" << match.moves.size()       << "\"" << "]" << "\n";
    
    if (!termination.empty())
        ss << "[Termination " << "\"" << termination << "\"" << "]" << "\n";

    if (match.whiteEngine.tc.fixed_time != 0)  
        ss << "[TimeControl " << "\"" << match.whiteEngine.tc.fixed_time << "/move" << "\"" << "]" << "\n";
    else 
        ss << "[TimeControl " << "\"" << match.whiteEngine.tc << "\"" << "]" << "\n";
    // clang-format on
    ss << "\n";
    std::stringstream illegalMove;

    if (!match.legal)
    {
        illegalMove << ", other side makes an illegal move: "
                    << match.moves[match.moves.size() - 1].move;
    }

    Board b = Board(match.fen);

    int moveCount = 3;
    for (size_t i = 0; i < match.moves.size(); i++)
    {
        const MoveData data = match.moves[i];

        std::stringstream nodesString, timeString;
        timeString << std::fixed << std::setprecision(3) << data.elapsedMillis / 1000.0 << "s";

        if (gameOptions.pgn.trackNodes)
        {
            nodesString << " n=" << data.nodes;
        }

        if (moveCount % 2 != 0)
            ss << moveCount / 2 << "."
               << " " << MoveToSan(b, convertUciToMove(b, data.move)) << " {" << data.scoreString
               << "/" << data.depth << nodesString.str() << " " << timeString.str()
               << illegalMove.str() << "}";
        else
        {
            ss << " " << MoveToSan(b, convertUciToMove(b, data.move)) << " {" << data.scoreString
               << "/" << data.depth << nodesString.str() << " " << timeString.str()
               << illegalMove.str() << "}";

            if (i == match.moves.size() - 1)
                break;

            if (i % 7 == 0)
                ss << "\n";
            else
                ss << " ";
        };

        if (!match.legal && i == match.moves.size() - 2)
        {
            break;
        }

        b.makeMove(convertUciToMove(b, data.move));

        moveCount++;
    }

    ss << " " << result << "\n";

    pgn = ss.str();
}

std::string PgnBuilder::getPGN() const
{
    return pgn;
}
