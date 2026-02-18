#include <game/pgn/pgn_builder.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <matchmaking/output/output.hpp>

namespace fastchess::pgn {

namespace str {
template <typename T>
std::string to_string(const T& obj) {
    std::stringstream ss;
    ss << obj;
    return ss.str();
}
}  // namespace str

PgnBuilder::PgnBuilder(const config::Pgn& pgn_config, const MatchData& match, std::size_t round_id)
    : pgn_config_(pgn_config), match_(match) {
    const auto& white_player = match.players.white;
    const auto& black_player = match.players.black;

    const auto is_frc_variant = match.variant == VariantType::FRC;

    pgn_generator_.addHeader("Event", pgn_config_.event_name);
    pgn_generator_.addHeader("Site", pgn_config_.site);
    pgn_generator_.addHeader("Date", match_.date);
    pgn_generator_.addHeader("Round", std::to_string(round_id));
    pgn_generator_.addHeader("White", white_player.config.name);
    pgn_generator_.addHeader("Black", black_player.config.name);
    pgn_generator_.addHeader("Result", getResultFromWhiteMatch(white_player));

    if (match_.fen != chess::constants::STARTPOS || is_frc_variant) {
        pgn_generator_.addHeader("SetUp", "1");
        pgn_generator_.addHeader("FEN", match_.fen);
    }

    if (is_frc_variant) {
#ifdef USE_CUTE
        pgn_generator_.addHeader("Variant", "fischerandom");
#else
        pgn_generator_.addHeader("Variant", "Chess960");
#endif
    }

    if (!pgn_config_.min) {
        pgn_generator_.addHeader("GameDuration", match_.duration);
        pgn_generator_.addHeader("GameStartTime", match_.start_time);
        pgn_generator_.addHeader("GameEndTime", match_.end_time);
        pgn_generator_.addHeader("PlyCount", std::to_string(match_.moves.size()));
        pgn_generator_.addHeader("Termination", convertMatchTermination(match_.termination));

        if (white_player.config.limit.tc == black_player.config.limit.tc) {
            pgn_generator_.addHeader("TimeControl", str::to_string(white_player.config.limit.tc));
        } else {
            pgn_generator_.addHeader("WhiteTimeControl", str::to_string(white_player.config.limit.tc));
            pgn_generator_.addHeader("BlackTimeControl", str::to_string(black_player.config.limit.tc));
        }
    }

    const auto opening = getOpeningClassification(is_frc_variant);

    if (!pgn_config_.min && opening) {
        pgn_generator_.addHeader("ECO", opening->eco);
        pgn_generator_.addHeader("Opening", opening->name);
    }

    pgn_generator_.setResult(getResultFromWhiteMatch(white_player));

    const auto firstIllegal = match_.moves.begin() != match_.moves.end() && !match_.moves.begin()->legal;

    if (firstIllegal) {
        pgn_generator_.addMove("", match_.reason + ": " + match_.moves.begin()->move);
        return;
    }

    chess::Board board = chess::Board();
    board.set960(is_frc_variant);
    board.setFen(match_.fen);

    const std::size_t move_number = int(board.sideToMove() == chess::Color::BLACK) + 2 * board.fullMoveNumber() - 1;

    pgn_generator_.setBlackStarts(board.sideToMove() == chess::Color::BLACK);
    pgn_generator_.setStartMoveNumber(move_number);

    for (auto it = match_.moves.begin(); it != match_.moves.end(); ++it) {
        // check if next move is illegal
        const auto next_illegal = std::next(it) != match_.moves.end() && !std::next(it)->legal;

        const auto last     = std::next(it) == match_.moves.end() || next_illegal;
        const auto move_str = moveNotation(board, it->move);
        const auto cmt_str  = createComment(*it, *std::next(it), next_illegal, last);

        pgn_generator_.addMove(move_str, cmt_str);

        board.makeMove<true>(chess::uci::uciToMove(board, it->move));

        if (next_illegal) {
            break;
        }
    }
}

std::optional<Opening> PgnBuilder::getOpeningClassification(bool is_frc_variant) const {
    chess::Board opening_board;
    opening_board.set960(is_frc_variant);
    opening_board.setFen(match_.fen);

    if (is_frc_variant) {
        return std::nullopt;
    }

    auto find_opening = [](const std::string_view fen) -> std::optional<Opening> {
        if (auto it = EPD_TO_OPENING.find(fen); it != EPD_TO_OPENING.end()) return it->second;
        return std::nullopt;
    };

    auto current_opening = find_opening(opening_board.getFen(false));

    for (const auto& move : match_.moves) {
        if (!move.legal) break;

        opening_board.makeMove<true>(chess::uci::uciToMove(opening_board, move.move));

        if (auto opening = find_opening(opening_board.getFen(false))) {
            current_opening = opening;
        }
    }

    return current_opening;
}

std::string PgnBuilder::moveNotation(chess::Board& board, const std::string& move) const noexcept {
    switch (pgn_config_.notation) {
        case NotationType::SAN:
            return chess::uci::moveToSan(board, chess::uci::uciToMove(board, move));
        case NotationType::LAN:
            return chess::uci::moveToLan(board, chess::uci::uciToMove(board, move));
        default:
            return move;
    }
}

std::string PgnBuilder::createComment(const MoveData& move, const MoveData& next_move, bool illegal,
                                      bool last) noexcept {
    std::string info_lines;

    if (!move.additional_lines.empty()) {
        for (const auto& line : move.additional_lines) {
            info_lines += info_lines.empty() ? "" : ", ";
            info_lines += "line=\"" + line + "\"";
        }
    }

    if (!pgn_config_.min) {
        if (move.book) {
            return addComment("book");
        }

        const auto match_str = illegal ? match_.reason + ": " + next_move.move : match_.reason;

        return addComment(
            (move.score_string + "/" + std::to_string(move.depth)) + " " + formatTime(move.elapsed_millis),
            pgn_config_.track_timeleft ? "tl=" + formatTime(move.timeleft) : "",            //
            pgn_config_.track_latency ? "latency=" + formatTime(move.latency) : "",         //
            pgn_config_.track_nodes ? "n=" + std::to_string(move.nodes) : "",               //
            pgn_config_.track_seldepth ? "sd=" + std::to_string(move.seldepth) : "",        //
            pgn_config_.track_nps ? "nps=" + std::to_string(move.nps) : "",                 //
            pgn_config_.track_hashfull ? "hashfull=" + std::to_string(move.hashfull) : "",  //
            pgn_config_.track_tbhits ? "tbhits=" + std::to_string(move.tbhits) : "",        //
            pgn_config_.track_pv ? "pv=\"" + move.pv + "\"" : "",                           //
            info_lines.empty() ? "" : info_lines,                                           //
            last ? match_str : ""                                                           //
        );
    }

    return "";
}

std::string PgnBuilder::getResultFromWhiteMatch(const MatchData::PlayerInfo& white) noexcept {
    switch (white.result) {
        case chess::GameResult::WIN:
            return "1-0";
        case chess::GameResult::LOSE:
            return "0-1";
        case chess::GameResult::DRAW:
            return "1/2-1/2";
        default:
            return "*";
    }
}

std::string PgnBuilder::convertMatchTermination(const MatchTermination& res) noexcept {
    switch (res) {
        case MatchTermination::NORMAL:
            return "normal";
        case MatchTermination::ADJUDICATION:
            return "adjudication";
        case MatchTermination::DISCONNECT:
            return "abandoned";
        case MatchTermination::STALL:
            return "abandoned";
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

}  // namespace fastchess::pgn
