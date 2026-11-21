#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <chess.hpp>

namespace fastchess::book {
struct Opening {
    Opening() = default;
    Opening(const std::string& fen_epd, const std::vector<chess::Move>& moves) : fen_epd(fen_epd), moves(moves) {}

    std::string fen_epd            = chess::constants::STARTPOS;
    std::vector<chess::Move> moves = {};
};

// True when a game can be played from the position: one king of each color, no pawn on the first or last rank, the
// side that just moved not in check and the side to move in check by two pieces at most.
inline bool isPlayablePosition(const chess::Board& board) {
    using namespace chess;

    if (board.pieces(PieceType::KING, Color::WHITE).count() != 1) return false;
    if (board.pieces(PieceType::KING, Color::BLACK).count() != 1) return false;
    if (!(board.pieces(PieceType::PAWN) & (Bitboard(Rank::RANK_1) | Bitboard(Rank::RANK_8))).empty()) return false;
    if (board.isAttacked(board.kingSq(~board.sideToMove()), board.sideToMove())) return false;

    return board.checkers().count() <= 2;
}

// Sets the board to an opening position given as a FEN, or as an EPD line when it has a ';' (as the match reads
// it), and tells whether it was read and can be played.
inline bool setPlayablePosition(chess::Board& board, std::string_view fen_epd) {
    const bool is_fen = fen_epd.find(';') == std::string_view::npos;
    const bool read   = is_fen ? board.setFen(fen_epd) : board.setEpd(fen_epd);
    return read && isPlayablePosition(board);
}
}  // namespace fastchess::book
