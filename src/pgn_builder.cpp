#include "pgn_builder.hpp"

#include <iomanip>  // std::setprecision
#include <sstream>

#include "matchmaking/output/output_factory.hpp"

namespace fast_chess {

PgnBuilder::PgnBuilder(const MatchData &match, const CMD::GameManagerOptions &game_options) {
    this->match_ = match;
    this->game_options_ = game_options;

    PlayerInfo white_player, black_player;

    if (match.players.first.color == Chess::Color::WHITE) {
        white_player = match.players.first;
        black_player = match.players.second;
    } else {
        white_player = match.players.second;
        black_player = match.players.first;
    }

    addHeader("Event", "Fast Chess");
    addHeader("Site", game_options_.site);
    addHeader("Round", std::to_string(match_.round));
    addHeader("White", white_player.config.name);
    addHeader("Black", black_player.config.name);
    addHeader("Date", match_.date);
    addHeader("Result", getResultFromMatch(match_));
    addHeader("FEN", match_.fen);
    addHeader("GameDuration", match_.duration);
    addHeader("GameStartTime", match_.start_time);
    addHeader("GameEndTime", match_.end_time);
    addHeader("PlyCount", std::to_string(match_.moves.size()));
    addHeader("Termination", match_.termination);
    addHeader("TimeControl", white_player.config.limit.tc);

    Chess::Board board = Chess::Board(match_.fen);
    std::size_t move_number = 0;

    while (move_number < match_.moves.size()) {
        auto move = match_.moves[move_number];
        addMove(board, move, move_number + 1);

        if (match_.termination == "illegal move" && move_number == match_.moves.size()) {
            break;
        }

        board.makeMove(board.uciToMove(move.move));

        move_number++;
    }
}

template <typename T>
void PgnBuilder::addHeader(const std::string &name, const T &value) {
    pgn_ << "[" << name << " \"" << value << "\"]\n";
}

void PgnBuilder::addMove(Chess::Board &board, const MoveData &move, std::size_t move_number) {
    pgn_ << (move_number % 2 == 1 ? std::to_string(move_number / 2 + 1) + ". " : "")
         << board.san(board.uciToMove(move.move)) << addComment(formatTime(move.elapsed_millis))
         << " ";
}

std::string PgnBuilder::getResultFromMatch(const MatchData &match) const {
    if (match.players.first.result == Chess::GameResult::WIN) {
        return "1-0";
    } else if (match.players.second.result == Chess::GameResult::WIN) {
        return "0-1";
    } else {
        return "1/2-1/2";
    }
}

}  // namespace fast_chess