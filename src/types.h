#pragma once

#include <cstdint>
#include <unordered_map>

using Bitboard = uint64_t;

static constexpr int N_SQ = 64;
static constexpr int64_t PING_TIMEOUT_THRESHOLD = 1000;

enum Piece : uint8_t
{
    WHITEPAWN,
    WHITEKNIGHT,
    WHITEBISHOP,
    WHITEROOK,
    WHITEQUEEN,
    WHITEKING,
    BLACKPAWN,
    BLACKKNIGHT,
    BLACKBISHOP,
    BLACKROOK,
    BLACKQUEEN,
    BLACKING,
    NONE
};

enum PieceType : uint8_t
{
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONETYPE
};

// clang-format off
enum Square : uint8_t {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    NO_SQ
};

// clang-format on

enum CastlingRight : uint8_t
{
    WK = 1,
    WQ = 2,
    BK = 4,
    BQ = 8
};

enum Color : uint8_t
{
    WHITE,
    BLACK,
    NO_COLOR
};

static std::unordered_map<char, Piece> charToPiece({{'P', WHITEPAWN},
                                                    {'N', WHITEKNIGHT},
                                                    {'B', WHITEBISHOP},
                                                    {'R', WHITEROOK},
                                                    {'Q', WHITEQUEEN},
                                                    {'K', WHITEKING},
                                                    {'p', BLACKPAWN},
                                                    {'n', BLACKKNIGHT},
                                                    {'b', BLACKBISHOP},
                                                    {'r', BLACKROOK},
                                                    {'q', BLACKQUEEN},
                                                    {'k', BLACKING},
                                                    {'.', NONE}});

static std::unordered_map<Piece, char> pieceToChar({{WHITEPAWN, 'P'},
                                                    {WHITEKNIGHT, 'N'},
                                                    {WHITEBISHOP, 'B'},
                                                    {WHITEROOK, 'R'},
                                                    {WHITEQUEEN, 'Q'},
                                                    {WHITEKING, 'K'},
                                                    {BLACKPAWN, 'p'},
                                                    {BLACKKNIGHT, 'n'},
                                                    {BLACKBISHOP, 'b'},
                                                    {BLACKROOK, 'r'},
                                                    {BLACKQUEEN, 'q'},
                                                    {BLACKING, 'k'},
                                                    {NONE, '.'}});

#define INCR_OP_ON(T)                                                                                                  \
    constexpr inline T &operator++(T &p)                                                                               \
    {                                                                                                                  \
        return p = static_cast<T>(static_cast<int>(p) + 1);                                                            \
    }                                                                                                                  \
    constexpr inline T operator++(T &p, int)                                                                           \
    {                                                                                                                  \
        auto old = p;                                                                                                  \
        ++p;                                                                                                           \
        return old;                                                                                                    \
    }

INCR_OP_ON(Piece)
INCR_OP_ON(Square)
INCR_OP_ON(PieceType)

#undef INCR_OP_ON

#define BASE_OP_ON(N, T)                                                                                               \
    inline constexpr N operator+(N s, T d)                                                                             \
    {                                                                                                                  \
        return N(int(s) + int(d));                                                                                     \
    }                                                                                                                  \
    inline constexpr N operator-(N s, T d)                                                                             \
    {                                                                                                                  \
        return N(int(s) - int(d));                                                                                     \
    }                                                                                                                  \
    inline constexpr N &operator+=(N &s, T d)                                                                          \
    {                                                                                                                  \
        return s = s + d;                                                                                              \
    }                                                                                                                  \
    inline constexpr N &operator-=(N &s, T d)                                                                          \
    {                                                                                                                  \
        return s = s - d;                                                                                              \
    }

#undef BASE_OP_ON

inline Color colorOf(Piece p)
{
    if (p == NONE)
        return NO_COLOR;
    return Color(p / 6);
}
