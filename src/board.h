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

    Table<Bitboard, N_SQ, N_SQ> SQUARES_BETWEEN_BB;
    Color sideToMove = WHITE;

    uint8_t castlingRights = 15;

    Square enPassantSquare = NO_SQ;

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

    void loadFen(const std::string &fen);

    void makeMove(Move move);

    void unmakeMove(Move move);

    Bitboard us(Color c) const;

    Bitboard allBB() const;

    Square KingSQ(Color c) const;

    template <PieceType type, Color color> Bitboard pieces() const;

    template <Color color> Bitboard pieces(PieceType type) const;

    template <PieceType type> Bitboard pieces(Color color) const;

    Bitboard pieces(PieceType type, Color color) const;

    Piece pieceAt(Square square) const;

    uint64_t getHash() const;

    uint64_t zobristHash() const;

    bool isRepetition(int draw) const;

    friend std::ostream &operator<<(std::ostream &os, const Board &b);

  private:
    Table<Piece, N_SQ> board = {};
    Table<Bitboard, 2, 6> pieceBB = {};
    std::vector<State> prevBoards;
    std::vector<uint64_t> hashHistory;

    int halfMoveClock;
    int fullMoveNumber;

    uint64_t hashKey = 0;

    void removeCastlingRightsAll(Color c);

    void removeCastlingRightsRook(Square sq);

    void initializeLookupTables();

    bool isKingAttacked(Color c, Square sq) const;

    void placePiece(Piece piece, Square sq);

    void removePiece(Piece piece, Square sq);

    uint64_t updateKeyPiece(Piece piece, Square sq) const;

    uint64_t updateKeyEnPassant(Square sq) const;

    uint64_t updateKeyCastling() const;

    uint64_t updateKeySideToMove() const;
};

template <PieceType type, Color color> Bitboard Board::pieces() const
{
    return pieceBB[color][type];
}

template <Color color> Bitboard Board::pieces(PieceType type) const
{
    return pieceBB[color][type];
}

template <PieceType type> Bitboard Board::pieces(Color color) const
{
    return pieceBB[color][type];
}

std::string uciMove(Move move);

Move convertUciToMove(const std::string &input);