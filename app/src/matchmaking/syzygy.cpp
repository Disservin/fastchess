#include <matchmaking/syzygy.hpp>

#include <core/filesystem/file_system.hpp>

#include <pyrrhic/tbprobe.h>

#include <cassert>
#include <stdexcept>

namespace fastchess {

int initSyzygy(const std::string_view syzygyDirs) {
    const bool success = tb_init(syzygyDirs.data());
    if (!success) {
        return 0;
    }

    return TB_LARGEST;
}

void tearDownSyzgy() { tb_free(); }

bool canProbeSyzgyWdl(const chess::Board& board) {
    if (board.halfMoveClock() != 0) {
        return false;
    }
    if (!board.castlingRights().isEmpty()) {
        return false;
    }
    if (board.occ().count() > TB_LARGEST) {
        return false;
    }

    return true;
}

chess::GameResult probeSyzygyWdl(const chess::Board& board) {
    assert(canProbeSyzgyWdl(board));
    // We now assume that the half move clock is 0 and the castling rights are empty.
    // If these conditions are not fulfilled, the probe result may be incorrect.

    const unsigned probeResult =
        tb_probe_wdl(board.us(chess::Color::WHITE).getBits(), board.us(chess::Color::BLACK).getBits(),
                     board.pieces(chess::PieceType::KING).getBits(), board.pieces(chess::PieceType::QUEEN).getBits(),
                     board.pieces(chess::PieceType::ROOK).getBits(), board.pieces(chess::PieceType::BISHOP).getBits(),
                     board.pieces(chess::PieceType::KNIGHT).getBits(), board.pieces(chess::PieceType::PAWN).getBits(),
                     board.enpassantSq().index(), board.sideToMove() == chess::Color::WHITE);

    if (probeResult == TB_RESULT_FAILED) {
        return chess::GameResult::NONE;
    }

    switch (probeResult) {
        case TB_WIN:
            return chess::GameResult::WIN;

        case TB_CURSED_WIN:
        case TB_BLESSED_LOSS:
        case TB_DRAW:
            return chess::GameResult::DRAW;

        case TB_LOSS:
            return chess::GameResult::LOSE;
    }

    assert(("This should be unreachable!", false));
    return chess::GameResult::NONE;
}

}  // namespace fastchess
