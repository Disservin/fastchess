#include "pgn_builder.hpp"

#include <iomanip>  // std::setprecision
#include <sstream>

#include "matchmaking/output/output_factory.hpp"

namespace fast_chess {

PgnBuilder::PgnBuilder(const MatchData &match, const CMD::GameManagerOptions &game_options) {
    this->match_ = match;
    this->game_options_ = game_options;

    addHeader("Event", "Fast Chess");
    addHeader("Site", game_options_.site);
    addHeader("Date", match_.date);
    addHeader("Round", std::to_string(match_.round));

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

void PgnBuilder::addHeader(const std::string &name, const std::string &value) {
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