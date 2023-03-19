#include "pgn_builder.hpp"

#include <iomanip> // std::setprecision
#include <sstream>

#include "chess/board.hpp"

namespace fast_chess
{

PgnBuilder::PgnBuilder(const MatchData &match, const CMD::GameManagerOptions &game_options_,
                       const bool saveTime)
{
    const std::string result = resultToString(match);
    const std::string termination = match.termination;
    std::stringstream ss;

    PlayerInfo white_player, black_player;

    if (match.players.first.color == WHITE)
    {
        white_player = match.players.first;
        black_player = match.players.second;
    }
    else
    {
        white_player = match.players.second;
        black_player = match.players.first;
    }

    // clang-format off
    ss << "[Event "         << "\"" << game_options_.event_name << "\"" << "]" << "\n";
    ss << "[Site "          << "\"" << game_options_.site       << "\"" << "]" << "\n";
    ss << "[Date "          << "\"" << match.date               << "\"" << "]" << "\n";
    ss << "[Round "         << "\"" << match.round              << "\"" << "]" << "\n";
    ss << "[White "         << "\"" << white_player.name        << "\"" << "]" << "\n";
    ss << "[Black "         << "\"" << black_player.name        << "\"" << "]" << "\n";
    ss << "[Result "        << "\"" << result                   << "\"" << "]" << "\n";
    ss << "[FEN "           << "\"" << match.fen                << "\"" << "]" << "\n";
    ss << "[GameDuration "  << "\"" << match.duration           << "\"" << "]" << "\n";
    ss << "[GameEndTime "   << "\"" << match.end_time           << "\"" << "]" << "\n";
    ss << "[GameStartTime " << "\"" << match.start_time         << "\"" << "]" << "\n";
    ss << "[PlyCount "      << "\"" << match.moves.size()       << "\"" << "]" << "\n";

    if (!termination.empty())
        ss << "[Termination " << "\"" << termination << "\"" << "]" << "\n";

    if (white_player.config.tc.fixed_time != 0)
        ss << "[TimeControl " << "\"" << white_player.config.tc.fixed_time << "/move" << "\"" <<
        "]" << "\n";
    else
        ss << "[TimeControl " << "\"" << white_player.config.tc << "\"" << "]" << "\n";
    // clang-format on
    ss << "\n";
    std::stringstream illegalMove;

    if (!match.legal)
    {
        illegalMove << ", other side makes an illegal move: "
                    << match.moves[match.moves.size() - 1].move;
    }

    Board b = Board(match.fen);

    int move_count = 3;
    for (size_t i = 0; i < match.moves.size(); i++)
    {
        const MoveData data = match.moves[i];

        std::stringstream nodesString, seldepthString, timeString;

        if (saveTime)
        {
            timeString << std::fixed << std::setprecision(3) << data.elapsed_millis / 1000.0 << "s";
        }

        if (game_options_.pgn.track_nodes)
        {
            nodesString << " n=" << data.nodes;
        }

        if (game_options_.pgn.track_seldepth)
        {
            seldepthString << " sd=" << data.seldepth;
        }

        const std::string move =
            MoveToRep(b, convertUciToMove(b, data.move), game_options_.pgn.notation != "san");

        if (move_count % 2 != 0)
            ss << move_count / 2 << "."
               << " " << move << " {" << data.score_string << "/" << data.depth << nodesString.str()
               << seldepthString.str() << " " << timeString.str() << illegalMove.str() << "}";
        else
        {
            ss << " " << move << " {" << data.score_string << "/" << data.depth << nodesString.str()
               << seldepthString.str() << " " << timeString.str() << illegalMove.str() << "}";

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

        move_count++;
    }

    ss << " " << result << "\n";

    pgn_ = ss.str();
}

std::string PgnBuilder::getPGN() const
{
    return pgn_;
}

} // namespace fast_chess
