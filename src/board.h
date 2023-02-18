#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "helper.h"
#include "move.h"
#include "types.h"

class Board
{
  public:
    Board();
    Board(Board &&) = default;
    Board(const Board &) = default;
    Board &operator=(Board &&) = default;
    Board &operator=(const Board &) = default;
    ~Board();

  private:
    Table<Piece, N_SQ> board;
    Table<Bitboard, 2, 6> pieceBB;
    Color sideToMove;
    uint8_t castlingRights;
    Square enPassantSquare;
    int halfMoveClock;
    int fullMoveNumber;

    bool isKingAttacked(Color c, Square sq);

    void load_fen(const std::string &fen);

    void place_piece(Piece piece, Square sq);
    void remove_piece(Piece piece, Square sq);

    bool make_move(Move move);
    void unmake_move(Move move);

    // Returns the piece at a given square on the board
    inline Piece piece_at(Square square) const
    {
        return board[square];
    }

    Bitboard us(Color c);
    Bitboard allBB();

    template <PieceType type, Color color> Bitboard pieces() const
    {
        return pieceBB[color][type];
    }

    template <Color color> Bitboard pieces(PieceType type) const
    {
        return pieceBB[color][type];
    }

    template <PieceType type> Bitboard pieces(Color color) const
    {
        return pieceBB[color][type];
    }

    inline Bitboard pieces(PieceType type, Color color) const
    {
        return pieceBB[color][type];
    }
};