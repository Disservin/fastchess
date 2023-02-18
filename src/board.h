#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "helper.h"
#include "move.h"
#include "types.h"

struct State
{
    Square enPassant{};
    uint8_t castling{};
    uint8_t halfMove{};
    Piece capturedPiece = NONE;
    State(Square enpassantCopy = {}, uint8_t castlingRightsCopy = {}, uint8_t halfMoveCopy = {},
          Piece capturedPieceCopy = NONE)
        : enPassant(enpassantCopy), castling(castlingRightsCopy), halfMove(halfMoveCopy),
          capturedPiece(capturedPieceCopy)
    {
    }
};

class Board
{
  public:
    Board();
    Board(Board &&) = default;
    Board(const Board &) = default;
    Board &operator=(Board &&) = default;
    Board &operator=(const Board &) = default;
    ~Board();

    // Movegen vars
    Bitboard pinD = {};
    Bitboard pinHV = {};
    Bitboard occEnemy = {};
    Bitboard occUs = {};
    Bitboard occAll = {};
    Bitboard seen = {};
    Bitboard enemyEmptyBB = {};
    Bitboard checkMask = DEFAULT_CHECKMASK;

    int doubleCheck = 0;

    Color sideToMove = WHITE;

    uint8_t castlingRights = 15;

    Square enPassantSquare = NO_SQ;

    void load_fen(const std::string &fen);

    void make_move(Move move);
    void unmake_move(Move move);

    Table<Bitboard, N_SQ, N_SQ> SQUARES_BETWEEN_BB;

    Bitboard us(Color c);
    Bitboard allBB();

    Square KingSQ(Color c) const;

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

    friend std::ostream &operator<<(std::ostream &os, const Board &b);

  private:
    Table<Piece, N_SQ> board = {};
    Table<Bitboard, 2, 6> pieceBB = {};
    std::vector<State> prev_boards;

    int halfMoveClock;
    int fullMoveNumber;

    void removeCastlingRightsAll(Color c);

    void removeCastlingRightsRook(Square sq);

    void initializeLookupTables();

    bool isKingAttacked(Color c, Square sq);

    void place_piece(Piece piece, Square sq);
    void remove_piece(Piece piece, Square sq);

    // Returns the piece at a given square on the board
    inline Piece piece_at(Square square) const
    {
        return board[square];
    }
};

std::string uciMove(Move move);