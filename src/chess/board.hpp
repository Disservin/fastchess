#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "chess/helper.hpp"
#include "chess/move.hpp"
#include "chess/types.hpp"
#include "matchmaking/match_data.hpp"

namespace fast_chess {

struct State {
    Square enpassant{};
    uint8_t castling{};
    uint8_t halfMove{};
    Piece captured_piece = NONE;

    State(Square _enpassant = {}, uint8_t _castling = {}, uint8_t _halfMove = {},
          Piece _captured_piece = NONE)
        : enpassant(_enpassant),
          castling(_castling),
          halfMove(_halfMove),
          captured_piece(_captured_piece) {}
};

class Board {
   public:
    Board();

    explicit Board(const std::string &fen);

    void loadFen(const std::string &fen);
    std::string getFen() const;

    bool makeMove(Move move);
    void unmakeMove(Move move);

    Bitboard us(Color c) const;
    Bitboard allBB() const;

    Square KingSQ(Color c) const;

    template <PieceType type, Color color>
    Bitboard pieces() const;
    template <Color color>
    Bitboard pieces(PieceType type) const;
    template <PieceType type>
    Bitboard pieces(Color color) const;
    Bitboard pieces(PieceType type, Color color) const;

    Piece pieceAt(Square square) const;

    uint64_t getHash() const;
    uint64_t zobristHash() const;

    Color getSideToMove() const;
    Square getEnpassantSquare() const;
    uint8_t getCastlingRights() const;
    Bitboard getSquaresBetweenBB(int sq_1, int sq_2) const;

    bool isChess960() const;
    bool isRepetition() const;

    /// @brief
    /// @return GameResult::DRAW or GameResult::LOSE or GameResult::NONE
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
    void initializeLookupTables();

    void placePiece(Piece piece, Square sq);
    void removePiece(Piece piece, Square sq);

    void removecastling_rightsAll(Color c);
    void removecastling_rightsRook(Square sq);

    uint64_t updateKeyPiece(Piece piece, Square sq) const;
    uint64_t updateKeyEnPassant(Square sq) const;
    uint64_t updateKeyCastling() const;
    uint64_t updateKeyside_to_move() const;

    Table<Piece, N_SQ> board_ = {};
    Table<Bitboard, 2, 6> pieceBB_ = {};
    Table<Bitboard, N_SQ, N_SQ> squares_between_bb_;

    std::vector<State> prev_boards_;
    std::vector<uint64_t> hash_history_;

    uint64_t hash_key_ = 0;

    int half_move_clock_;
    int full_move_number_;

    Square enpassant_square_ = NO_SQ;

    uint8_t castling_rights_ = 15;

    Color side_to_move_ = WHITE;

    bool chess960_ = false;
};

template <PieceType type, Color color>
Bitboard Board::pieces() const {
    return pieceBB_[color][type];
}

template <Color color>
Bitboard Board::pieces(PieceType type) const {
    return pieceBB_[color][type];
}

template <PieceType type>
Bitboard Board::pieces(Color color) const {
    return pieceBB_[color][type];
}

std::string uciMove(Move move);

Move convertUciToMove(const Board &board, const std::string &input);

std::string MoveToRep(Board &b, Move move, bool isLan = false);

std::string resultToString(const MatchData &match);

}  // namespace fast_chess
