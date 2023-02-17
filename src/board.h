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
    Bitboard pieceBB[2][6];
    Color sideToMove;
    uint8_t castlingRights;
    Square enPassantSquare;
    int halfMoveClock;
    int fullMoveNumber;

    void load_fen(const std::string &fen);

    void place_piece(Piece piece, Square sq);
    void remove_piece(Piece piece, Square sq);

    void make_move(Move move);
    void unmake_move(Move move);

    // Returns the piece at a given square on the board
    Piece piece_at(Square square) const
    {
        return board[square];
    }

    template <Color color, PieceType type> constexpr Bitboard pieces() const
    {
        return pieceBB[color][type];
    }

    template <Color color> constexpr Bitboard pieces(PieceType type) const
    {
        return pieceBB[color][type];
    }

    template <PieceType type> constexpr Bitboard pieces(Color color) const
    {
        return pieceBB[color][type];
    }

    constexpr Bitboard pieces(Color color, PieceType type) const
    {
        return pieceBB[color][type];
    }
};

Board::Board()
{
}

Board::~Board()
{
}