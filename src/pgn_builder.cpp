#include <pgn_builder.hpp>

#include <sstream>

#include <matchmaking/output/output.hpp>

namespace fast_chess {

PgnBuilder::PgnBuilder(const MatchData &match, const cmd::TournamentOptions &tournament_options,
                       std::size_t round_id) {
    match_        = match;
    game_options_ = tournament_options;

    PlayerInfo white_player, black_player;

    if (match.players.first.color == chess::Color::WHITE) {
        white_player = match.players.first;
        black_player = match.players.second;
    } else {
        white_player = match.players.second;
        black_player = match.players.first;
    }

    addHeader("Event", tournament_options.event_name);
    addHeader("Site", game_options_.site);
    addHeader("Round", std::to_string(round_id));
    addHeader("White", white_player.config.name);
    addHeader("Black", black_player.config.name);
    addHeader("Date", match_.date);
    addHeader("Result", getResultFromMatch(match_));
    addHeader("FEN", match_.fen);
    addHeader("GameDuration", match_.duration);
    addHeader("GameStartTime", match_.start_time);
    addHeader("GameEndTime", match_.end_time);
    addHeader("PlyCount", std::to_string(match_.moves.size()));
    addHeader("Termination", convertMatchTermination(match_.termination));
    addHeader("TimeControl", white_player.config.limit.tc);

    chess::Board board      = chess::Board(match_.fen);
    std::size_t move_number = 0;

    while (move_number < match_.moves.size()) {
        const auto illegal = match_.termination == MatchTermination::ILLEGAL_MOVE &&
                             move_number == match_.moves.size() - 1;

        const auto move = match_.moves[move_number];

        addMove(board, move, move_number + 1, illegal);

        if (illegal) {
            break;
        }

        board.makeMove(chess::uci::uciToMove(board, move.move));

        move_number++;
    }

    pgn_ << "\n";

    // create the pgn lines and assert that the line length is below 80 characters
    // otherwise move the move onto the next line
    std::size_t line_length = 0;
    for (auto &move : moves_) {
        assert(move.size() <= 80);
        if (line_length + move.size() > LINE_LENGTH) {
            pgn_ << "\n";
            line_length = 0;
        }
        pgn_ << (line_length == 0 ? "" : " ") << move;
        line_length += move.size();
    }
}

template <typename T>
void PgnBuilder::addHeader(std::string_view name, const T &value) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (value.empty()) {
            return;
        }
    }
    pgn_ << "[" << name << " \"" << value << "\"]\n";
}

std::string PgnBuilder::moveNotation(chess::Board &board, const std::string &move) const {
    if (game_options_.pgn.notation == NotationType::SAN) {
        return chess::uci::moveToSan(board, chess::uci::uciToMove(board, move));
    } else if (game_options_.pgn.notation == NotationType::LAN) {
        return chess::uci::moveToLan(board, chess::uci::uciToMove(board, move));
    } else {
        return move;
    }
}

void PgnBuilder::addMove(chess::Board &board, const MoveData &move, std::size_t move_number,
                         bool illegal) {
    std::stringstream ss;

    ss << (move_number % 2 == 1 ? std::to_string(move_number / 2 + 1) + ". " : "");
    ss << (illegal ? move.move : moveNotation(board, move.move));

    ss << addComment(
        (move.score_string + "/" + std::to_string(move.depth)), formatTime(move.elapsed_millis),
        game_options_.pgn.track_nodes ? "nodes," + std::to_string(move.nodes) : "",
        game_options_.pgn.track_seldepth ? "seldepth, " + std::to_string(move.seldepth) : "",
        game_options_.pgn.track_nps ? "nps, " + std::to_string(move.nps) : "",
        match_.moves.size() == move_number ? match_.reason : "");

    moves_.emplace_back(ss.str());
}

std::string PgnBuilder::getResultFromMatch(const MatchData &match) {
    if (match.players.first.result == chess::GameResult::WIN) {
        return "1-0";
    } else if (match.players.second.result == chess::GameResult::WIN) {
        return "0-1";
    } else {
        return "1/2-1/2";
    }
}

std::string PgnBuilder::convertMatchTermination(const MatchTermination &res) {
    switch (res) {
        case MatchTermination::ADJUDICATION:
            return "adjudication";
        case MatchTermination::TIMEOUT:
            return "time forfeit";
        case MatchTermination::ILLEGAL_MOVE:
            return "illegal move";
        case MatchTermination::INTERRUPT:
            return "unterminated";
        default:
            return "";
    }
}

}  // namespace fast_chess