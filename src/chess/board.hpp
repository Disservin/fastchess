#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "helper.hpp"
#include "move.hpp"
#include "types.hpp"

namespace fast_chess
{

enum class GameResult
{
    WHITE_WIN,
    BLACK_WIN,
    DRAW,
    NONE
};

struct State
{
    Square enPassant{};
    uint8_t castling{};
    uint8_t halfMove{};
    Piece capturedPiece = NONE;

    State(Square enpassantCopy = {}, uint8_t castling_rightsCopy = {}, uint8_t halfMoveCopy = {},
          Piece capturedPieceCopy = NONE)
        : enPassant(enpassantCopy), castling(castling_rightsCopy), halfMove(halfMoveCopy),
          capturedPiece(capturedPieceCopy)
    {
    }
};

class Board
{
  public:
    Board();

    Board(const std::string &fen);

    Table<Bitboard, N_SQ, N_SQ> squares_between_bb_;
    Color side_to_move_ = WHITE;

    uint8_t castling_rights_ = 15;

    Square enpassant_square_ = NO_SQ;

    bool chess960_ = false;

    void loadFen(const std::string &fen);
    std::string getFen() const;

    bool makeMove(Move move);

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

    bool isRepetition() const;

    GameResult isGameOver();

    Bitboard attacksByPiece(PieceType pt, Square sq, Color c, Bitboard occ) const;

    bool isSquareAttacked(Color c, Square sq) const;

    static PieceType typeOfPiece(const Piece piece);
    static File squareFile(Square sq);
    static Rank squareRank(Square sq);

    static Square fileRankSquare(File f, Rank r);
    static Piece make_piece(PieceType type, Color c);

    // returns diagonal of given square
    static uint8_t diagonalOf(Square sq);

    // returns anti diagonal of given square
    static uint8_t anti_diagonalOf(Square sq);

    static bool sameColor(int sq1, int sq2);

    static Color colorOf(Piece p);

    Color colorOf(Square loc) const;

    static uint8_t squareDistance(Square a, Square b);

    friend std::ostream &operator<<(std::ostream &os, const Board &b);

  private:
    Table<Piece, N_SQ> board_ = {};
    Table<Bitboard, 2, 6> pieceBB_ = {};
    std::vector<State> prev_boards_;
    std::vector<uint64_t> hash_history_;

    int half_move_clock_;
    int full_move_number_;

    uint64_t hash_key_ = 0;

    void removecastling_rightsAll(Color c);

    void removecastling_rightsRook(Square sq);

    void initializeLookupTables();

    void placePiece(Piece piece, Square sq);

    void removePiece(Piece piece, Square sq);

    uint64_t updateKeyPiece(Piece piece, Square sq) const;

    uint64_t updateKeyEnPassant(Square sq) const;

    uint64_t updateKeyCastling() const;

    uint64_t updateKeyside_to_move() const;
};

template <PieceType type, Color color> Bitboard Board::pieces() const
{
    return pieceBB_[color][type];
}

template <Color color> Bitboard Board::pieces(PieceType type) const
{
    return pieceBB_[color][type];
}

template <PieceType type> Bitboard Board::pieces(Color color) const
{
    return pieceBB_[color][type];
}

std::string uciMove(Move move);

Move convertUciToMove(const Board &board, const std::string &input);

std::string MoveToRep(Board &b, Move move, bool isLan = false);

std::string resultToString(GameResult result);

} // namespace fast_chess
