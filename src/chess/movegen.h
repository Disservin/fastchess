#pragma once

#include <cassert>
#include <iterator>

#include "attacks.h"
#include "board.h"
#include "helper.h"
#include "types.h"

struct Movelist
{
    Move list[MAX_MOVES] = {};
    uint8_t size = 0;
    typedef Move *iterator;
    typedef const Move *const_iterator;

    void Add(Move move)
    {
        list[size] = move;
        size++;
    }

    inline constexpr Move &operator[](int i)
    {
        return list[i];
    }

    inline int find(Move m) const
    {
        for (int i = 0; i < size; i++)
        {
            if (list[i] == m)
                return i;
        }
        return -1;
    }

    iterator begin()
    {
        return (std::begin(list));
    }

    const_iterator begin() const
    {
        return (std::begin(list));
    }

    iterator end()
    {
        auto it = std::begin(list);
        std::advance(it, size);
        return it;
    }

    const_iterator end() const
    {
        auto it = std::begin(list);
        std::advance(it, size);
        return it;
    }
};

namespace Movegen
{

template <Color c> Bitboard pawnLeftAttacks(const Bitboard pawns)
{
    return c == WHITE ? (pawns << 7) & ~MASK_FILE[FILE_H] : (pawns >> 7) & ~MASK_FILE[FILE_A];
}

template <Color c> Bitboard pawnRightAttacks(const Bitboard pawns)
{
    return c == WHITE ? (pawns << 9) & ~MASK_FILE[FILE_A] : (pawns >> 9) & ~MASK_FILE[FILE_H];
}

/********************
 * Creates the checkmask.
 * A checkmask is the path from the enemy checker to our king.
 * Knight and pawns get themselves added to the checkmask, otherwise the path is added.
 * When there is no check at all all bits are set (DEFAULT_CHECKMASK)
 *******************/
template <Color c> Bitboard DoCheckmask(Board &board, Square sq)
{
    const Bitboard Occ = board.occAll;
    Bitboard checks = 0ULL;
    const Bitboard pawn_mask = board.pieces<PAWN, ~c>() & PawnAttacks(sq, c);
    const Bitboard knight_mask = board.pieces<KNIGHT, ~c>() & KnightAttacks(sq);
    const Bitboard bishop_mask = (board.pieces<BISHOP, ~c>() | board.pieces<QUEEN, ~c>()) & BishopAttacks(sq, Occ);
    const Bitboard rook_mask = (board.pieces<ROOK, ~c>() | board.pieces<QUEEN, ~c>()) & RookAttacks(sq, Occ);

    /********************
     * We keep track of the amount of checks, in case there are
     * two checks on the board only the king can move!
     *******************/
    board.doubleCheck = 0;

    if (pawn_mask)
    {
        checks |= pawn_mask;
        board.doubleCheck++;
    }
    if (knight_mask)
    {
        checks |= knight_mask;
        board.doubleCheck++;
    }
    if (bishop_mask)
    {
        const int8_t index = lsb(bishop_mask);

        // Now we add the path!
        checks |= board.SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
        board.doubleCheck++;
    }
    if (rook_mask)
    {
        /********************
         * 3nk3/4P3/8/8/8/8/8/2K1R3 w - - 0 1, pawn promotes to queen or rook and
         * suddenly the same piecetype gives check two times
         * in that case we have a double check and can return early
         * because king moves dont require the checkmask.
         *******************/
        if (popcount(rook_mask) > 1)
        {
            board.doubleCheck = 2;
            return checks;
        }

        const int8_t index = lsb(rook_mask);

        // Now we add the path!
        checks |= board.SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
        board.doubleCheck++;
    }

    if (!checks)
        return DEFAULT_CHECKMASK;

    return checks;
}

/********************
 * Create the pinmask
 * We define the pin mask as the path from the enemy pinner
 * through the pinned piece to our king.
 * We assume that the king can do rook and bishop moves and generate the attacks
 * for these in case we hit an enemy rook/bishop/queen we might have a possible pin.
 * We need to confirm the pin because there could be 2 or more pieces from our king to
 * the possible pinner. We do this by simply using the popcount
 * of our pieces that lay on the pin mask, if it is only 1 piece then that piece is pinned.
 *******************/
template <Color c> Bitboard DoPinMaskRooks(Board &board, Square sq)
{
    Bitboard rook_mask = (board.pieces<ROOK, ~c>() | board.pieces<QUEEN, ~c>()) & RookAttacks(sq, board.occEnemy);

    Bitboard pinHV = 0ULL;
    while (rook_mask)
    {
        const Square index = poplsb(rook_mask);
        const Bitboard possible_pin = (board.SQUARES_BETWEEN_BB[sq][index] | (1ULL << index));
        if (popcount(possible_pin & board.occUs) == 1)
            pinHV |= possible_pin;
    }
    return pinHV;
}

template <Color c> Bitboard DoPinMaskBishops(Board &board, Square sq)
{
    Bitboard bishop_mask = (board.pieces<BISHOP, ~c>() | board.pieces<QUEEN, ~c>()) & BishopAttacks(sq, board.occEnemy);

    Bitboard pinD = 0ULL;

    while (bishop_mask)
    {
        const Square index = poplsb(bishop_mask);
        const Bitboard possible_pin = (board.SQUARES_BETWEEN_BB[sq][index] | (1ULL << index));
        if (popcount(possible_pin & board.occUs) == 1)
            pinD |= possible_pin;
    }

    return pinD;
}

/********************
 * Seen squares
 * We keep track of all attacked squares by the enemy
 * this is used for king move generation.
 *******************/
template <Color c> Bitboard seenSquares(Board &board)
{
    const Square kSq = board.KingSQ(~c);

    const Bitboard pawns = board.pieces<PAWN, c>();
    Bitboard knights = board.pieces<KNIGHT, c>();
    Bitboard queens = board.pieces<QUEEN, c>();
    Bitboard bishops = board.pieces<BISHOP, c>() | queens;
    Bitboard rooks = board.pieces<ROOK, c>() | queens;

    // Remove our king
    board.occAll &= ~(1ULL << kSq);

    Bitboard seen = pawnLeftAttacks<c>(pawns) | pawnRightAttacks<c>(pawns);

    while (knights)
    {
        const Square index = poplsb(knights);
        seen |= KnightAttacks(index);
    }
    while (bishops)
    {
        const Square index = poplsb(bishops);
        seen |= BishopAttacks(index, board.occAll);
    }
    while (rooks)
    {
        const Square index = poplsb(rooks);
        seen |= RookAttacks(index, board.occAll);
    }

    const Square index = lsb(board.pieces<KING, c>());
    seen |= KingAttacks(index);

    // Place our King back
    board.occAll |= (1ULL << kSq);

    return seen;
}

/********************
 * Creates the pinmask and checkmask
 * setup important variables that we use for move generation.
 *******************/
template <Color c> void init(Board &board, Square sq)
{
    board.occUs = board.us(c);
    board.occEnemy = board.us(~c);
    board.occAll = board.occUs | board.occEnemy;
    board.enemyEmptyBB = ~board.occUs;

    board.seen = seenSquares<~c>(board);
    board.checkMask = DoCheckmask<c>(board, sq);
    board.pinHV = DoPinMaskRooks<c>(board, sq);
    board.pinD = DoPinMaskBishops<c>(board, sq);
}

/// @brief shift a mask in a direction
/// @tparam direction
/// @param b
/// @return
template <Direction direction> constexpr Bitboard shift(const Bitboard b)
{
    switch (direction)
    {
    case NORTH:
        return b << 8;
    case SOUTH:
        return b >> 8;
    case NORTH_WEST:
        return (b & ~MASK_FILE[0]) << 7;
    case WEST:
        return (b & ~MASK_FILE[0]) >> 1;
    case SOUTH_WEST:
        return (b & ~MASK_FILE[0]) >> 9;
    case NORTH_EAST:
        return (b & ~MASK_FILE[7]) << 9;
    case EAST:
        return (b & ~MASK_FILE[7]) << 1;
    case SOUTH_EAST:
        return (b & ~MASK_FILE[7]) >> 7;
    }
    assert(false);
    return 0;
}

template <Color c> void LegalPawnMovesAll(Board &board, Movelist &movelist)
{
    const Bitboard pawns_mask = board.pieces<PAWN, c>();

    constexpr Direction UP = c == WHITE ? NORTH : SOUTH;
    constexpr Direction DOWN = c == BLACK ? NORTH : SOUTH;
    constexpr Direction DOWN_LEFT = c == BLACK ? NORTH_EAST : SOUTH_WEST;
    constexpr Direction DOWN_RIGHT = c == BLACK ? NORTH_WEST : SOUTH_EAST;
    constexpr Bitboard RANK_BEFORE_PROMO = c == WHITE ? MASK_RANK[RANK_7] : MASK_RANK[RANK_2];
    constexpr Bitboard RANK_PROMO = c == WHITE ? MASK_RANK[RANK_8] : MASK_RANK[RANK_1];
    constexpr Bitboard doublePushRank = c == WHITE ? MASK_RANK[RANK_3] : MASK_RANK[RANK_6];

    // These pawns can maybe take Left or Right
    const Bitboard pawnsLR = pawns_mask & ~board.pinHV;

    const Bitboard unpinnedpawnsLR = pawnsLR & ~board.pinD;
    const Bitboard pinnedpawnsLR = pawnsLR & board.pinD;

    Bitboard Lpawns = (pawnLeftAttacks<c>(unpinnedpawnsLR)) | (pawnLeftAttacks<c>(pinnedpawnsLR) & board.pinD);
    Bitboard Rpawns = (pawnRightAttacks<c>(unpinnedpawnsLR)) | (pawnRightAttacks<c>(pinnedpawnsLR) & board.pinD);

    // Prune moves that dont capture a piece and are not on the checkmask.
    Lpawns &= board.occEnemy & board.checkMask;
    Rpawns &= board.occEnemy & board.checkMask;

    // These pawns can walk Forward
    const Bitboard pawnsHV = pawns_mask & ~board.pinD;

    const Bitboard pawnsPinnedHV = pawnsHV & board.pinHV;
    const Bitboard pawnsUnPinnedHV = pawnsHV & ~board.pinHV;

    // Prune moves that are blocked by a piece
    const Bitboard singlePushUnpinned = shift<UP>(pawnsUnPinnedHV) & ~board.occAll;
    const Bitboard singlePushPinned = shift<UP>(pawnsPinnedHV) & board.pinHV & ~board.occAll;

    // Prune moves that are not on the checkmask.
    Bitboard singlePush = (singlePushUnpinned | singlePushPinned) & board.checkMask;

    Bitboard doublePush = ((shift<UP>(singlePushUnpinned & doublePushRank) & ~board.occAll) |
                           (shift<UP>(singlePushPinned & doublePushRank) & ~board.occAll)) &
                          board.checkMask;

    /********************
     * Add promotion moves.
     * These are always generated unless we only want quiet moves.
     *******************/
    if (pawns_mask & RANK_BEFORE_PROMO)
    {
        Bitboard Promote_Left = Lpawns & RANK_PROMO;
        Bitboard Promote_Right = Rpawns & RANK_PROMO;
        Bitboard Promote_Move = singlePush & RANK_PROMO;

        while (Promote_Move)
        {
            Square to = poplsb(Promote_Move);
            movelist.Add({to + DOWN, to, QUEEN});
            movelist.Add({to + DOWN, to, ROOK});
            movelist.Add({to + DOWN, to, BISHOP});
            movelist.Add({to + DOWN, to, KNIGHT});
        }

        while (Promote_Right)
        {
            Square to = poplsb(Promote_Right);
            movelist.Add({to + DOWN_LEFT, to, QUEEN});
            movelist.Add({to + DOWN_LEFT, to, ROOK});
            movelist.Add({to + DOWN_LEFT, to, BISHOP});
            movelist.Add({to + DOWN_LEFT, to, KNIGHT});
        }

        while (Promote_Left)
        {
            Square to = poplsb(Promote_Left);
            movelist.Add({to + DOWN_RIGHT, to, QUEEN});
            movelist.Add({to + DOWN_RIGHT, to, ROOK});
            movelist.Add({to + DOWN_RIGHT, to, BISHOP});
            movelist.Add({to + DOWN_RIGHT, to, KNIGHT});
        }
    }

    // Remove the promotion pawns
    singlePush &= ~RANK_PROMO;
    Lpawns &= ~RANK_PROMO;
    Rpawns &= ~RANK_PROMO;

    /********************
     * Add single pushs.
     *******************/
    while (singlePush)
    {
        const Square to = poplsb(singlePush);
        movelist.Add({to + DOWN, to, NONETYPE});
    }

    /********************
     * Add double pushs.
     *******************/
    while (doublePush)
    {
        const Square to = poplsb(doublePush);
        movelist.Add({to + DOWN + DOWN, to, NONETYPE});
    }

    /********************
     * Add right pawn captures.
     *******************/
    while (Rpawns)
    {
        const Square to = poplsb(Rpawns);
        movelist.Add({to + DOWN_LEFT, to, NONETYPE});
    }

    /********************
     * Add left pawn captures.
     *******************/
    while (Lpawns)
    {
        const Square to = poplsb(Lpawns);
        movelist.Add({to + DOWN_RIGHT, to, NONETYPE});
    }

    /********************
     * Add en passant captures.
     *******************/
    if (board.enPassantSquare != NO_SQ)
    {
        const Square ep = board.enPassantSquare;
        const Square epPawn = ep + DOWN;

        const Bitboard epMask = (1ull << epPawn) | (1ull << ep);

        /********************
         * In case the en passant square and the enemy pawn
         * that just moved are not on the checkmask
         * en passant is not available.
         *******************/
        if ((board.checkMask & epMask) == 0)
            return;

        const Square kSQ = board.KingSQ(c);
        const Bitboard kingMask = (1ull << kSQ) & MASK_RANK[Board::squareRank(epPawn)];
        const Bitboard enemyQueenRook = board.pieces<ROOK, ~c>() | board.pieces<QUEEN, ~c>();

        const bool isPossiblePin = kingMask && enemyQueenRook;
        Bitboard epBB = PawnAttacks(ep, ~c) & pawnsLR;

        /********************
         * For one en passant square two pawns could potentially take there.
         *******************/
        while (epBB)
        {
            const Square from = poplsb(epBB);
            const Square to = ep;

            /********************
             * If the pawn is pinned but the en passant square is not on the
             * pin mask then the move is illegal.
             *******************/
            if ((1ULL << from) & board.pinD && !(board.pinD & (1ull << ep)))
                continue;

            const Bitboard connectingPawns = (1ull << epPawn) | (1ull << from);

            /********************
             * 7k/4p3/8/2KP3r/8/8/8/8 b - - 0 1
             * If e7e5 there will be a potential ep square for us on e6.
             * However we cannot take en passant because that would put our king
             * in check. For this scenario we check if theres an enemy rook/queen
             * that would give check if the two pawns were removed.
             * If thats the case then the move is illegal and we can break immediately.
             *******************/
            if (isPossiblePin && (RookAttacks(kSQ, board.occAll & ~connectingPawns) & enemyQueenRook) != 0)
                break;

            movelist.Add({from, to, NONETYPE});
        }
    }
}

inline Bitboard LegalKnightMoves(Square sq, Bitboard movableSquare)
{
    return KnightAttacks(sq) & movableSquare;
}

inline Bitboard LegalBishopMoves(const Board &board, Square sq, Bitboard movableSquare)
{
    // The Bishop is pinned diagonally thus can only move diagonally.
    if (board.pinD & (1ULL << sq))
        return BishopAttacks(sq, board.occAll) & movableSquare & board.pinD;
    return BishopAttacks(sq, board.occAll) & movableSquare;
}

inline Bitboard LegalRookMoves(const Board &board, Square sq, Bitboard movableSquare)
{
    // The Rook is pinned horizontally thus can only move horizontally.
    if (board.pinHV & (1ULL << sq))
        return RookAttacks(sq, board.occAll) & movableSquare & board.pinHV;
    return RookAttacks(sq, board.occAll) & movableSquare;
}

inline Bitboard LegalQueenMoves(const Board &board, Square sq, Bitboard movableSquare)
{
    Bitboard moves = 0ULL;
    if (board.pinD & (1ULL << sq))
        moves |= BishopAttacks(sq, board.occAll) & movableSquare & board.pinD;
    else if (board.pinHV & (1ULL << sq))
        moves |= RookAttacks(sq, board.occAll) & movableSquare & board.pinHV;
    else
    {
        moves |= RookAttacks(sq, board.occAll) & movableSquare;
        moves |= BishopAttacks(sq, board.occAll) & movableSquare;
    }

    return moves;
}

inline Bitboard LegalKingMoves(const Board &board, Square sq)
{
    return KingAttacks(sq) & board.enemyEmptyBB & ~board.seen;
}

template <Color c> Bitboard LegalKingMovesCastling(const Board &board, Square sq)
{
    Bitboard moves = KingAttacks(sq) & board.enemyEmptyBB & ~board.seen;
    const Bitboard emptyAndNotAttacked = ~board.seen & ~board.occAll;

    // clang-format off
		switch (c)
		{
		case WHITE:
			if (board.castlingRights & WK
				&& emptyAndNotAttacked & (1ULL << SQ_F1)
				&& emptyAndNotAttacked & (1ull << SQ_G1))
				moves |= (1ULL << SQ_G1);
			if (board.castlingRights & WQ
				&& emptyAndNotAttacked & (1ULL << SQ_D1)
				&& emptyAndNotAttacked & (1ull << SQ_C1)
				&& (1ull << SQ_B1) & ~board.occAll)
				moves |= (1ULL << SQ_C1);
			break;
		case BLACK:
			if (board.castlingRights & BK
				&& emptyAndNotAttacked & (1ULL << SQ_F8)
				&& emptyAndNotAttacked & (1ull << SQ_G8))
				moves |= (1ULL << SQ_G8);
			if (board.castlingRights & BQ
				&& emptyAndNotAttacked & (1ULL << SQ_D8)
				&& emptyAndNotAttacked & (1ull << SQ_C8)
				&& (1ull << SQ_B8) & ~board.occAll)
				moves |= (1ULL << SQ_C8);

			break;
		default:
			return moves;
		}
    // clang-format on

    return moves;
}

// all legal moves for a position
template <Color c> void legalmoves(Board &board, Movelist &movelist)
{
    /********************
     * The size of the movelist might not
     * be 0! This is done on purpose since it enables
     * you to append new move types to any movelist.
     *******************/

    init<c>(board, board.KingSQ(c));

    assert(board.doubleCheck <= 2);

    /********************
     * Moves have to be on the checkmask
     *******************/
    Bitboard movableSquare = board.checkMask;

    /********************
     * Slider, Knights and King moves can only go to enemy or empty squares.
     *******************/
    movableSquare &= board.enemyEmptyBB;

    {
        const Square from = board.KingSQ(c);
        Bitboard moves;

        if (!board.castlingRights || board.checkMask != DEFAULT_CHECKMASK)
            moves = LegalKingMoves(board, from);
        else
            moves = LegalKingMovesCastling<c>(board, from);

        while (moves)
        {
            Square to = poplsb(moves);
            movelist.Add({from, to, NONETYPE});
        }
    }

    /********************
     * Early return for double check as described earlier
     *******************/
    if (board.doubleCheck == 2)
        return;

    /********************
     * Prune knights that are pinned since these cannot move.
     *******************/
    Bitboard knights_mask = board.pieces<KNIGHT, c>() & ~(board.pinD | board.pinHV);

    /********************
     * Prune horizontally pinned bishops
     *******************/
    Bitboard bishops_mask = board.pieces<BISHOP, c>() & ~board.pinHV;

    /********************
     * Prune diagonally pinned rooks
     *******************/
    Bitboard rooks_mask = board.pieces<ROOK, c>() & ~board.pinD;

    /********************
     * Prune double pinned queens
     *******************/
    Bitboard queens_mask = board.pieces<QUEEN, c>() & ~(board.pinD & board.pinHV);

    /********************
     * Add the moves to the movelist.
     *******************/
    LegalPawnMovesAll<c>(board, movelist);

    while (knights_mask)
    {
        const Square from = poplsb(knights_mask);
        Bitboard moves = LegalKnightMoves(from, movableSquare);
        while (moves)
        {
            const Square to = poplsb(moves);
            movelist.Add({from, to, NONETYPE});
        }
    }

    while (bishops_mask)
    {
        const Square from = poplsb(bishops_mask);
        Bitboard moves = LegalBishopMoves(board, from, movableSquare);
        while (moves)
        {
            const Square to = poplsb(moves);
            movelist.Add({from, to, NONETYPE});
        }
    }

    while (rooks_mask)
    {
        const Square from = poplsb(rooks_mask);
        Bitboard moves = LegalRookMoves(board, from, movableSquare);
        while (moves)
        {
            const Square to = poplsb(moves);
            movelist.Add({from, to, NONETYPE});
        }
    }

    while (queens_mask)
    {
        const Square from = poplsb(queens_mask);
        Bitboard moves = LegalQueenMoves(board, from, movableSquare);
        while (moves)
        {
            const Square to = poplsb(moves);
            movelist.Add({from, to, NONETYPE});
        }
    }
}

// all legal moves for a position
template <Color c> bool hasLegalMoves(Board &board, Movelist &movelist)
{
    /********************
     * The size of the movelist might not
     * be 0! This is done on purpose since it enables
     * you to append new move types to any movelist.
     *******************/

    init<c>(board, board.KingSQ(c));

    assert(board.doubleCheck <= 2);

    /********************
     * Moves have to be on the checkmask
     *******************/
    Bitboard movableSquare = board.checkMask;

    /********************
     * Slider, Knights and King moves can only go to enemy or empty squares.
     *******************/
    movableSquare &= board.enemyEmptyBB;

    {
        Square from = board.KingSQ(c);
        Bitboard moves;

        if (!board.castlingRights || board.checkMask != DEFAULT_CHECKMASK)
            moves = LegalKingMoves(board, from);
        else
            moves = LegalKingMovesCastling<c>(board, from);

        while (moves)
        {
            return true;
        }
    }

    /********************
     * Early return for double check as described earlier
     *******************/
    if (board.doubleCheck == 2)
        return false;

    /********************
     * Prune knights that are pinned since these cannot move.
     *******************/
    Bitboard knights_mask = board.pieces<KNIGHT, c>() & ~(board.pinD | board.pinHV);

    /********************
     * Prune horizontally pinned bishops
     *******************/
    Bitboard bishops_mask = board.pieces<BISHOP, c>() & ~board.pinHV;

    /********************
     * Prune diagonally pinned rooks
     *******************/
    Bitboard rooks_mask = board.pieces<ROOK, c>() & ~board.pinD;

    /********************
     * Prune double pinned queens
     *******************/
    Bitboard queens_mask = board.pieces<QUEEN, c>() & ~(board.pinD & board.pinHV);

    /********************
     * Add the moves to the movelist.
     *******************/
    const auto size = movelist.size;
    LegalPawnMovesAll<c>(board, movelist);

    if (movelist.size != size)
        return true;

    while (knights_mask)
    {
        const Square from = poplsb(knights_mask);
        Bitboard moves = LegalKnightMoves(from, movableSquare);
        while (moves)
        {
            return true;
        }
    }

    while (bishops_mask)
    {
        const Square from = poplsb(bishops_mask);
        Bitboard moves = LegalBishopMoves(board, from, movableSquare);
        while (moves)
        {
            return true;
        }
    }

    while (rooks_mask)
    {
        const Square from = poplsb(rooks_mask);
        Bitboard moves = LegalRookMoves(board, from, movableSquare);
        while (moves)
        {
            return true;
        }
    }

    while (queens_mask)
    {
        const Square from = poplsb(queens_mask);
        Bitboard moves = LegalQueenMoves(board, from, movableSquare);
        while (moves)
        {
            return true;
        }
    }

    return false;
}

inline bool hasLegalMoves(Board &board)
{
    Movelist moves;
    if (board.sideToMove == WHITE)
        return hasLegalMoves<WHITE>(board, moves);
    else
        return hasLegalMoves<BLACK>(board, moves);
}

/********************
 * Entry function for the
 * Color template.
 *******************/
inline void legalmoves(Board &board, Movelist &movelist)
{
    if (board.sideToMove == WHITE)
        legalmoves<WHITE>(board, movelist);
    else
        legalmoves<BLACK>(board, movelist);
}

} // namespace Movegen
