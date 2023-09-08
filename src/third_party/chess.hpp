/*
MIT License

Copyright (c) 2023 disservin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Source: https://github.com/Disservin/chess-library
*/

/*
VERSION: 0.1.8
*/

#ifndef CHESS_HPP
#define CHESS_HPP

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chess {

/****************************************************************************\
 * Type aliases                                                              *
\****************************************************************************/

using U64      = std::uint64_t;
using Bitboard = std::uint64_t;  // clear distinction between bitboard and U64

/****************************************************************************\
 * Enumerations                                                              *
\****************************************************************************/

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

enum class MoveGenType : uint8_t { ALL, CAPTURE, QUIET };

enum class Direction : int8_t {
    NORTH      = 8,
    WEST       = -1,
    SOUTH      = -8,
    EAST       = 1,
    NORTH_EAST = 9,
    NORTH_WEST = 7,
    SOUTH_WEST = -9,
    SOUTH_EAST = -7
};

enum class File { NO_FILE = -1, FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

enum class Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NO_RANK };

enum class PieceType : uint8_t { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE };

enum class Piece : uint8_t {
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
    BLACKKING,
    NONE
};

enum class Color : uint8_t { WHITE, BLACK, NONE };

enum class CastleSide : uint8_t { KING_SIDE, QUEEN_SIDE };

enum class GameResult { WIN, LOSE, DRAW, NONE };

enum class GameResultReason {
    CHECKMATE,
    STALEMATE,
    INSUFFICIENT_MATERIAL,
    FIFTY_MOVE_RULE,
    THREEFOLD_REPETITION,
    NONE
};

/****************************************************************************\
 * Enum Overloads                                                            *
\****************************************************************************/

#define ADD_BASE_OPERATORS_FOR(T)                                                        \
    constexpr T operator+(T vv_1, int vv_2) { return static_cast<T>(int(vv_1) + vv_2); } \
    constexpr T operator-(T vv_1, int vv_2) { return static_cast<T>(int(vv_1) - vv_2); } \
    constexpr T operator-(T vv) { return static_cast<T>(-int(vv)); }                     \
    constexpr T &operator+=(T &vv_1, int vv_2) { return vv_1 = vv_1 + vv_2; }            \
    constexpr T &operator-=(T &vv_1, int vv_2) { return vv_1 = vv_1 - vv_2; }

#define ADD_INCR_OPERATORS_FOR(T)                                               \
    constexpr T &operator++(T &vv) { return vv = static_cast<T>(int(vv) + 1); } \
    constexpr T &operator--(T &vv) { return vv = static_cast<T>(int(vv) - 1); } \
    constexpr T operator++(T &vv, int) {                                        \
        T result = vv;                                                          \
        ++vv;                                                                   \
        return result;                                                          \
    }                                                                           \
    constexpr T operator--(T &vv, int) {                                        \
        T result = vv;                                                          \
        --vv;                                                                   \
        return result;                                                          \
    }

constexpr Square operator+(Square sq, Direction dir) {
    return static_cast<Square>(static_cast<int8_t>(sq) + static_cast<int8_t>(dir));
}

constexpr Color operator~(Color c) { return Color(static_cast<uint8_t>(c) ^ 1); }

constexpr GameResult operator~(GameResult gm) {
    if (gm == GameResult::WIN)
        return GameResult::LOSE;
    else if (gm == GameResult::LOSE)
        return GameResult::WIN;
    else if (gm == GameResult::DRAW)
        return GameResult::DRAW;
    else
        return GameResult::NONE;
}

constexpr Square operator^(Square sq, int i) {
    return static_cast<Square>(static_cast<uint8_t>(sq) ^ static_cast<uint8_t>(i));
}

/****************************************************************************\
 * Constants                                                                 *
\****************************************************************************/

constexpr int MAX_SQ                 = 64;
constexpr int MAX_PIECE              = 12;
constexpr int MAX_MOVES              = 256;
constexpr Bitboard DEFAULT_CHECKMASK = 18446744073709551615ULL;

static const std::string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// clang-format off
const std::string squareToString[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};
// clang-format on

/// @brief Convert a char to the internal Piece representation
/// @param c
/// @return
constexpr Piece charToPiece(char c) {
    switch (c) {
        case 'P':
            return Piece::WHITEPAWN;
        case 'N':
            return Piece::WHITEKNIGHT;
        case 'B':
            return Piece::WHITEBISHOP;
        case 'R':
            return Piece::WHITEROOK;
        case 'Q':
            return Piece::WHITEQUEEN;
        case 'K':
            return Piece::WHITEKING;
        case 'p':
            return Piece::BLACKPAWN;
        case 'n':
            return Piece::BLACKKNIGHT;
        case 'b':
            return Piece::BLACKBISHOP;
        case 'r':
            return Piece::BLACKROOK;
        case 'q':
            return Piece::BLACKQUEEN;
        case 'k':
            return Piece::BLACKKING;
        case '.':
            return Piece::NONE;
        default:
            break;
    }
    return Piece::NONE;
}

/// @brief Convert a Piece to a char
/// @param piece
/// @return
constexpr char pieceToChar(Piece piece) {
    switch (piece) {
        case Piece::WHITEPAWN:
            return 'P';
        case Piece::WHITEKNIGHT:
            return 'N';
        case Piece::WHITEBISHOP:
            return 'B';
        case Piece::WHITEROOK:
            return 'R';
        case Piece::WHITEQUEEN:
            return 'Q';
        case Piece::WHITEKING:
            return 'K';
        case Piece::BLACKPAWN:
            return 'p';
        case Piece::BLACKKNIGHT:
            return 'n';
        case Piece::BLACKBISHOP:
            return 'b';
        case Piece::BLACKROOK:
            return 'r';
        case Piece::BLACKQUEEN:
            return 'q';
        case Piece::BLACKKING:
            return 'k';
        case Piece::NONE:
            break;
    }
    return '.';
}

static std::unordered_map<PieceType, char> PieceTypeToChar({{PieceType::PAWN, 'p'},
                                                            {PieceType::KNIGHT, 'n'},
                                                            {PieceType::BISHOP, 'b'},
                                                            {PieceType::ROOK, 'r'},
                                                            {PieceType::QUEEN, 'q'},
                                                            {PieceType::KING, 'k'}});

static std::unordered_map<char, PieceType> charToPieceType({{'n', PieceType::KNIGHT},
                                                            {'b', PieceType::BISHOP},
                                                            {'r', PieceType::ROOK},
                                                            {'q', PieceType::QUEEN},
                                                            {'k', PieceType::KING},
                                                            {'N', PieceType::KNIGHT},
                                                            {'B', PieceType::BISHOP},
                                                            {'R', PieceType::ROOK},
                                                            {'Q', PieceType::QUEEN},
                                                            {'K', PieceType::KING}});

/****************************************************************************\
 * Forward declarations                                                      *
\****************************************************************************/
class Board;

namespace utils {
/// @brief Converts a string to a square
/// @param squareStr
/// @return
[[nodiscard]] Square extractSquare(std::string_view squareStr);

/// @brief Makes a square from a file and rank
/// @param f
/// @param r
/// @return
[[nodiscard]] constexpr Square fileRankSquare(File f, Rank r);

/// @brief Get the rank of a square
/// @param sq
/// @return
[[nodiscard]] constexpr Rank squareRank(Square sq);

/// @brief Get the file of a square
/// @param a
/// @param b
/// @return
[[nodiscard]] int squareDistance(Square a, Square b);

/// @brief Get the PieceType of a piece
/// @param piece
/// @return
[[nodiscard]] constexpr PieceType typeOfPiece(Piece piece);
}  // namespace utils

/****************************************************************************\
 * Structs                                                                   *
\****************************************************************************/

/// @brief [Internal Usage] 16 bit bitfield
class BitField16 {
   public:
    BitField16() : value_(0) {}

    // Sets the value of the specified group to the given value
    void setGroupValue(uint16_t group_index, uint16_t group_value) {
        assert(group_value < 16 && "group_value must be less than 16");
        assert(group_index < 4 && "group_index must be less than 4");

        // calculate the bit position of the start of the group you want to set
        const uint16_t startBit = group_index * group_size_;
        const auto setMask      = static_cast<uint16_t>(group_value << startBit);

        // clear the bits in the group
        value_ &= ~(0xF << startBit);

        // set the bits in the group
        value_ |= setMask;
    }

    [[nodiscard]] uint16_t getGroup(uint16_t group_index) const {
        assert(group_index < 4 && "group_index must be less than 4");
        uint16_t startBit = group_index * group_size_;
        return (value_ >> startBit) & 0xF;
    }

    void clear() { value_ = 0; }
    [[nodiscard]] uint16_t get() const { return value_; }

   private:
    static constexpr uint16_t group_size_ = 4;  // size of each group
    uint16_t value_                       = 0;
};

class CastlingRights {
   public:
    template <Color color, CastleSide castle, File rook_file>
    void setCastlingRight() {
        int file = static_cast<uint16_t>(rook_file) + 1;

        castling_rights_.setGroupValue(2 * static_cast<int>(color) + static_cast<int>(castle),
                                       static_cast<uint16_t>(file));
    }

    void setCastlingRight(Color color, CastleSide castle, File rook_file) {
        int file = static_cast<uint16_t>(rook_file) + 1;

        castling_rights_.setGroupValue(2 * static_cast<int>(color) + static_cast<int>(castle),
                                       static_cast<uint16_t>(file));
    }

    void clearAllCastlingRights() { castling_rights_.clear(); }

    int clearCastlingRight(Color color, CastleSide castle) {
        castling_rights_.setGroupValue(2 * static_cast<int>(color) + static_cast<int>(castle), 0);

        switch (castle) {
            case CastleSide::KING_SIDE:
                return color == Color::WHITE ? 0 : 2;
            case CastleSide::QUEEN_SIDE:
                return color == Color::WHITE ? 1 : 3;
            default:
                assert(false);
                return -1;
        }
    }

    void clearCastlingRight(Color color) {
        castling_rights_.setGroupValue(2 * static_cast<int>(color), 0);
        castling_rights_.setGroupValue(2 * static_cast<int>(color) + 1, 0);
    }

    [[nodiscard]] bool isEmpty() const { return castling_rights_.get() == 0; }

    [[nodiscard]] bool hasCastlingRight(Color color) const {
        return castling_rights_.getGroup(2 * static_cast<int>(color)) != 0 ||
               castling_rights_.getGroup(2 * static_cast<int>(color) + 1) != 0;
    }

    [[nodiscard]] bool hasCastlingRight(Color color, CastleSide castle) const {
        return castling_rights_.getGroup(2 * static_cast<int>(color) + static_cast<int>(castle)) !=
               0;
    }

    [[nodiscard]] File getRookFile(Color color, CastleSide castle) const {
        return static_cast<File>(
            castling_rights_.getGroup(2 * static_cast<int>(color) + static_cast<int>(castle)) - 1);
    }

    [[nodiscard]] int getHashIndex() const {
        return hasCastlingRight(Color::WHITE, CastleSide::KING_SIDE) +
               2 * hasCastlingRight(Color::WHITE, CastleSide::QUEEN_SIDE) +
               4 * hasCastlingRight(Color::BLACK, CastleSide::KING_SIDE) +
               8 * hasCastlingRight(Color::BLACK, CastleSide::QUEEN_SIDE);
    }

   private:
    /*
     denotes the file of the rook that we castle to
     1248 1248 1248 1248
     0000 0000 0000 0000
     bq   bk   wq   wk
     3    2    1    0    // group index
     */
    BitField16 castling_rights_ = {};
};

struct State {
    U64 hash;
    CastlingRights castling;
    Square enpassant;
    uint8_t half_moves;
    Piece captured_piece;

    State(const U64 &hash, const CastlingRights &castling, const Square &enpassant,
          const uint8_t &half_moves, const Piece &captured_piece)
        : hash(hash),
          castling(castling),
          enpassant(enpassant),
          half_moves(half_moves),
          captured_piece(captured_piece) {}
};

struct Move {
   public:
    Move() = default;
    constexpr Move(uint16_t move) : move_(move), score_(0) {}

    /// @brief Creates a move from a source and target square.
    /// pt is the promotion piece, when you want to create a promotion move you must also
    /// pass PROMOTION as the MoveType template parameter.
    /// @tparam MoveType
    /// @param source
    /// @param target
    /// @param pt
    /// @return
    template <uint16_t MoveType = 0>
    [[nodiscard]] static constexpr Move make(Square source, Square target,
                                             PieceType pt = PieceType::KNIGHT) {
        return Move(MoveType + ((uint16_t(pt) - uint16_t(PieceType::KNIGHT)) << 12) +
                    uint16_t(source << 6) + uint16_t(target));
    }

    /// @brief Get the source square of the move.
    /// @return
    [[nodiscard]] constexpr Square from() const { return static_cast<Square>((move_ >> 6) & 0x3F); }

    /// @brief Get the target square of the move.
    /// @return
    [[nodiscard]] constexpr Square to() const { return static_cast<Square>(move_ & 0x3F); }

    /// @brief Get the type of the move. Can be NORMAL, PROMOTION, ENPASSANT or CASTLING.
    /// @return
    [[nodiscard]] constexpr uint16_t typeOf() const {
        return static_cast<uint16_t>(move_ & (3 << 14));
    }

    /// @brief Get the promotion piece of the move, should only be used if typeOf() returns
    /// PROMOTION.
    /// @return
    [[nodiscard]] constexpr PieceType promotionType() const {
        return static_cast<PieceType>(((move_ >> 12) & 3) + static_cast<int>(PieceType::KNIGHT));
    }

    /// @brief Set the score for a move. Useful if you later want to sort the moves.
    /// @param score
    constexpr void setScore(int16_t score) { score_ = score; }

    [[nodiscard]] constexpr uint16_t move() const { return move_; }
    [[nodiscard]] constexpr int16_t score() const { return score_; }

    bool operator==(const Move &rhs) const { return move_ == rhs.move_; }
    bool operator!=(const Move &rhs) const { return move_ != rhs.move_; }

    static constexpr uint16_t NO_MOVE   = 0;
    static constexpr uint16_t NULL_MOVE = 65;
    static constexpr uint16_t NORMAL    = 0;
    static constexpr uint16_t PROMOTION = 1 << 14;
    static constexpr uint16_t ENPASSANT = 2 << 14;
    static constexpr uint16_t CASTLING  = 3 << 14;

    friend std::ostream &operator<<(std::ostream &os, const Move &move);

   private:
    uint16_t move_;
    int16_t score_;
};

inline std::ostream &operator<<(std::ostream &os, const Move &move) {
    Square from_sq = move.from();
    Square to_sq   = move.to();

    os << squareToString[from_sq] << squareToString[to_sq];
    if (move.typeOf() == Move::PROMOTION) {
        os << PieceTypeToChar.at(move.promotionType());
    }

    return os;
}

struct Movelist {
   public:
    /// @brief Add a move to the end of the movelist.
    /// @param move
    constexpr void add(Move move) {
        assert(size_ < MAX_MOVES);
        moves_[size_++] = move;
    }

    /// @brief Checks if a move is in the movelist, returns the index of the move if it is found,
    /// otherwise -1.
    /// @param move
    /// @return
    constexpr int find(Move move) {
        for (int i = 0; i < size_; ++i) {
            if (moves_[i] == move) {
                return i;
            }
        }
        return -1;
    }

    /// @brief Return the number of moves in the movelist.
    /// @return
    [[nodiscard]] constexpr int size() const { return size_; }

    /// @brief Checks if the movelist is empty.
    /// @return
    [[nodiscard]] constexpr bool empty() const { return size_ == 0; }

    /// @brief Clears the movelist.
    constexpr void clear() { size_ = 0; }

    /// @brief Sorts the movelist by score in descending order. Uses std::stable_sort.
    inline void sort(int index = 0) {
        std::stable_sort(moves_ + index, moves_ + size_,
                         [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }

    constexpr Move operator[](int index) const { return moves_[index]; }
    constexpr Move &operator[](int index) { return moves_[index]; }

    typedef Move *iterator;
    typedef const Move *const_iterator;

    constexpr iterator begin() { return moves_; }
    constexpr iterator end() { return moves_ + size_; }

    [[nodiscard]] constexpr const_iterator begin() const { return moves_; }
    [[nodiscard]] constexpr const_iterator end() const { return moves_ + size_; }

   private:
    Move moves_[MAX_MOVES]{};
    int size_ = 0;
};

struct PgnMove {
    Move move;
    std::string comment;
};

/****************************************************************************\
 * Various utility functions used across the codebase                        *
\****************************************************************************/

namespace utils {

/// @brief https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
/// @param is
/// @param t
/// @return
inline std::istream &safeGetline(std::istream &is, std::string &t) {
    t.clear();

    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.

    std::istream::sentry se(is, true);
    std::streambuf *sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
                return is;
            case '\r':
                if (sb->sgetc() == '\n') sb->sbumpc();
                return is;
            case std::streambuf::traits_type::eof():
                // Also handle the case when the last line has no line ending
                if (t.empty()) is.setstate(std::ios::eofbit);
                return is;
            default:
                t += (char)c;
        }
    }
}

/// @brief Print a bitboard to the console.
/// @param bb
inline void printBitboard(Bitboard bb) {
    std::bitset<MAX_SQ> b(bb);
    std::string str_bitset = b.to_string();
    for (int i = 0; i < MAX_SQ; i += 8) {
        std::string x = str_bitset.substr(i, 8);
        reverse(x.begin(), x.end());
        std::cout << x << std::endl;
    }
    std::cout << '\n' << std::endl;
}

/// @brief Split a string into a vector of strings, using a delimiter.
/// @param string
/// @param delimiter
/// @return
[[nodiscard]] inline std::vector<std::string> splitString(const std::string &string,
                                                          const char &delimiter) {
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter)) seglist.emplace_back(segment);

    return seglist;
}

/// @brief Get the file of a square.
/// @param sq
/// @return
[[nodiscard]] constexpr File squareFile(Square sq) { return File(sq & 7); }

/// @brief Get the rank of a square.
/// @param sq
/// @return
[[nodiscard]] constexpr Rank squareRank(Square sq) { return Rank(sq >> 3); }

[[nodiscard]] inline int squareDistance(Square a, Square b) {
    return std::max(std::abs(int(squareFile(a)) - int(squareFile(b))),
                    std::abs(int(squareRank(a)) - int(squareRank(b))));
}

[[nodiscard]] constexpr int diagonalOf(Square sq) {
    return 7 + int(squareRank(sq)) - int(squareFile(sq));
}
[[nodiscard]] constexpr int antiDiagonalOf(Square sq) {
    return int(squareRank(sq)) + int(squareFile(sq));
}

static inline void ltrim(std::string &s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

[[nodiscard]] constexpr PieceType typeOfPiece(Piece piece) {
    if (piece == Piece::NONE) return PieceType::NONE;
    return static_cast<PieceType>(static_cast<int>(piece) % 6);
}

[[nodiscard]] constexpr Square relativeSquare(Color c, Square s) {
    return Square(s ^ (static_cast<int>(c) * 56));
}

/// @brief Make a piece from a color and a piece type.
/// @param c
/// @param pt
/// @return
[[nodiscard]] constexpr Piece makePiece(Color c, PieceType pt) {
    return static_cast<Piece>(static_cast<int>(c) * 6 + static_cast<int>(pt));
}

[[nodiscard]] constexpr Square fileRankSquare(File f, Rank r) {
    return static_cast<Square>((static_cast<int>(r) << 3) + static_cast<int>(f));
}

/// @brief Checks if two squares have the same color. I.e light or dark.
/// @param sq1
/// @param sq2
/// @return
[[nodiscard]] constexpr bool sameColor(Square sq1, Square sq2) {
    return ((9 * (sq1 ^ sq2)) & 8) == 0;
}

[[nodiscard]] inline Square extractSquare(std::string_view squareStr) {
    char letter = squareStr[0];
    int file    = letter - 96;
    int rank    = squareStr[1] - 48;
    int index   = (rank - 1) * 8 + file - 1;
    return Square(index);
}

/// @brief Checks if a square is on the back rank of a color.
/// @param sq
/// @param color
/// @return
[[nodiscard]] inline bool ourBackRank(Square sq, Color color) {
    if (color == Color::WHITE)
        return squareRank(sq) == Rank::RANK_1;
    else
        return squareRank(sq) == Rank::RANK_8;
}

}  // namespace utils

/****************************************************************************\
 * Wrapper around compiler functions                                         *
\****************************************************************************/

namespace builtin {
/// @brief Get the least significant bit of a U64.
/// @param b
/// @return
inline Square lsb(U64 b);

/// @brief Get the most significant bit of a U64.
/// @param b
/// @return
inline Square msb(U64 b);

#if defined(__GNUC__)  // GCC, Clang, ICC
inline Square lsb(U64 b) {
    assert(b);
    return Square(__builtin_ctzll(b));
}

inline Square msb(U64 b) {
    assert(b);
    return Square(63 ^ __builtin_clzll(b));
}

#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64
#include <intrin.h>
inline Square lsb(U64 b) {
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return (Square)idx;
}

inline Square msb(U64 b) {
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return (Square)idx;
}

#else  // MSVC, WIN32
#include <intrin.h>
inline Square lsb(U64 b) {
    unsigned long idx;

    if (b & 0xffffffff) {
        _BitScanForward(&idx, int32_t(b));
        return Square(idx);
    } else {
        _BitScanForward(&idx, int32_t(b >> 32));
        return Square(idx + 32);
    }
}

inline Square msb(U64 b) {
    unsigned long idx;

    if (b >> 32) {
        _BitScanReverse(&idx, int32_t(b >> 32));
        return Square(idx + 32);
    } else {
        _BitScanReverse(&idx, int32_t(b));
        return Square(idx);
    }
}

#endif

#else  // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

/// @brief Count the number of set bits in a U64.
/// @param mask
/// @return
inline int popcount(U64 mask) {
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

    return (uint8_t)_mm_popcnt_u64(mask);

#else  // Assumed gcc or compatible compiler

    return __builtin_popcountll(mask);

#endif
}

/// @brief Get the least significant bit of a U64 and pop it.
/// @param mask
/// @return
inline Square poplsb(Bitboard &mask) {
    Square s = lsb(mask);
    mask &= mask - 1;  // compiler optimizes this to _blsr_u64
    return Square(s);
}

}  // namespace builtin

/****************************************************************************\
 * Polyglot Zobrist Hash                                                     *
\****************************************************************************/
namespace zobrist {
static constexpr U64 RANDOM_ARRAY[781] = {
    0x9D39247E33776D41, 0x2AF7398005AAA5C7, 0x44DB015024623547, 0x9C15F73E62A76AE2,
    0x75834465489C0C89, 0x3290AC3A203001BF, 0x0FBBAD1F61042279, 0xE83A908FF2FB60CA,
    0x0D7E765D58755C10, 0x1A083822CEAFE02D, 0x9605D5F0E25EC3B0, 0xD021FF5CD13A2ED5,
    0x40BDF15D4A672E32, 0x011355146FD56395, 0x5DB4832046F3D9E5, 0x239F8B2D7FF719CC,
    0x05D1A1AE85B49AA1, 0x679F848F6E8FC971, 0x7449BBFF801FED0B, 0x7D11CDB1C3B7ADF0,
    0x82C7709E781EB7CC, 0xF3218F1C9510786C, 0x331478F3AF51BBE6, 0x4BB38DE5E7219443,
    0xAA649C6EBCFD50FC, 0x8DBD98A352AFD40B, 0x87D2074B81D79217, 0x19F3C751D3E92AE1,
    0xB4AB30F062B19ABF, 0x7B0500AC42047AC4, 0xC9452CA81A09D85D, 0x24AA6C514DA27500,
    0x4C9F34427501B447, 0x14A68FD73C910841, 0xA71B9B83461CBD93, 0x03488B95B0F1850F,
    0x637B2B34FF93C040, 0x09D1BC9A3DD90A94, 0x3575668334A1DD3B, 0x735E2B97A4C45A23,
    0x18727070F1BD400B, 0x1FCBACD259BF02E7, 0xD310A7C2CE9B6555, 0xBF983FE0FE5D8244,
    0x9F74D14F7454A824, 0x51EBDC4AB9BA3035, 0x5C82C505DB9AB0FA, 0xFCF7FE8A3430B241,
    0x3253A729B9BA3DDE, 0x8C74C368081B3075, 0xB9BC6C87167C33E7, 0x7EF48F2B83024E20,
    0x11D505D4C351BD7F, 0x6568FCA92C76A243, 0x4DE0B0F40F32A7B8, 0x96D693460CC37E5D,
    0x42E240CB63689F2F, 0x6D2BDCDAE2919661, 0x42880B0236E4D951, 0x5F0F4A5898171BB6,
    0x39F890F579F92F88, 0x93C5B5F47356388B, 0x63DC359D8D231B78, 0xEC16CA8AEA98AD76,
    0x5355F900C2A82DC7, 0x07FB9F855A997142, 0x5093417AA8A7ED5E, 0x7BCBC38DA25A7F3C,
    0x19FC8A768CF4B6D4, 0x637A7780DECFC0D9, 0x8249A47AEE0E41F7, 0x79AD695501E7D1E8,
    0x14ACBAF4777D5776, 0xF145B6BECCDEA195, 0xDABF2AC8201752FC, 0x24C3C94DF9C8D3F6,
    0xBB6E2924F03912EA, 0x0CE26C0B95C980D9, 0xA49CD132BFBF7CC4, 0xE99D662AF4243939,
    0x27E6AD7891165C3F, 0x8535F040B9744FF1, 0x54B3F4FA5F40D873, 0x72B12C32127FED2B,
    0xEE954D3C7B411F47, 0x9A85AC909A24EAA1, 0x70AC4CD9F04F21F5, 0xF9B89D3E99A075C2,
    0x87B3E2B2B5C907B1, 0xA366E5B8C54F48B8, 0xAE4A9346CC3F7CF2, 0x1920C04D47267BBD,
    0x87BF02C6B49E2AE9, 0x092237AC237F3859, 0xFF07F64EF8ED14D0, 0x8DE8DCA9F03CC54E,
    0x9C1633264DB49C89, 0xB3F22C3D0B0B38ED, 0x390E5FB44D01144B, 0x5BFEA5B4712768E9,
    0x1E1032911FA78984, 0x9A74ACB964E78CB3, 0x4F80F7A035DAFB04, 0x6304D09A0B3738C4,
    0x2171E64683023A08, 0x5B9B63EB9CEFF80C, 0x506AACF489889342, 0x1881AFC9A3A701D6,
    0x6503080440750644, 0xDFD395339CDBF4A7, 0xEF927DBCF00C20F2, 0x7B32F7D1E03680EC,
    0xB9FD7620E7316243, 0x05A7E8A57DB91B77, 0xB5889C6E15630A75, 0x4A750A09CE9573F7,
    0xCF464CEC899A2F8A, 0xF538639CE705B824, 0x3C79A0FF5580EF7F, 0xEDE6C87F8477609D,
    0x799E81F05BC93F31, 0x86536B8CF3428A8C, 0x97D7374C60087B73, 0xA246637CFF328532,
    0x043FCAE60CC0EBA0, 0x920E449535DD359E, 0x70EB093B15B290CC, 0x73A1921916591CBD,
    0x56436C9FE1A1AA8D, 0xEFAC4B70633B8F81, 0xBB215798D45DF7AF, 0x45F20042F24F1768,
    0x930F80F4E8EB7462, 0xFF6712FFCFD75EA1, 0xAE623FD67468AA70, 0xDD2C5BC84BC8D8FC,
    0x7EED120D54CF2DD9, 0x22FE545401165F1C, 0xC91800E98FB99929, 0x808BD68E6AC10365,
    0xDEC468145B7605F6, 0x1BEDE3A3AEF53302, 0x43539603D6C55602, 0xAA969B5C691CCB7A,
    0xA87832D392EFEE56, 0x65942C7B3C7E11AE, 0xDED2D633CAD004F6, 0x21F08570F420E565,
    0xB415938D7DA94E3C, 0x91B859E59ECB6350, 0x10CFF333E0ED804A, 0x28AED140BE0BB7DD,
    0xC5CC1D89724FA456, 0x5648F680F11A2741, 0x2D255069F0B7DAB3, 0x9BC5A38EF729ABD4,
    0xEF2F054308F6A2BC, 0xAF2042F5CC5C2858, 0x480412BAB7F5BE2A, 0xAEF3AF4A563DFE43,
    0x19AFE59AE451497F, 0x52593803DFF1E840, 0xF4F076E65F2CE6F0, 0x11379625747D5AF3,
    0xBCE5D2248682C115, 0x9DA4243DE836994F, 0x066F70B33FE09017, 0x4DC4DE189B671A1C,
    0x51039AB7712457C3, 0xC07A3F80C31FB4B4, 0xB46EE9C5E64A6E7C, 0xB3819A42ABE61C87,
    0x21A007933A522A20, 0x2DF16F761598AA4F, 0x763C4A1371B368FD, 0xF793C46702E086A0,
    0xD7288E012AEB8D31, 0xDE336A2A4BC1C44B, 0x0BF692B38D079F23, 0x2C604A7A177326B3,
    0x4850E73E03EB6064, 0xCFC447F1E53C8E1B, 0xB05CA3F564268D99, 0x9AE182C8BC9474E8,
    0xA4FC4BD4FC5558CA, 0xE755178D58FC4E76, 0x69B97DB1A4C03DFE, 0xF9B5B7C4ACC67C96,
    0xFC6A82D64B8655FB, 0x9C684CB6C4D24417, 0x8EC97D2917456ED0, 0x6703DF9D2924E97E,
    0xC547F57E42A7444E, 0x78E37644E7CAD29E, 0xFE9A44E9362F05FA, 0x08BD35CC38336615,
    0x9315E5EB3A129ACE, 0x94061B871E04DF75, 0xDF1D9F9D784BA010, 0x3BBA57B68871B59D,
    0xD2B7ADEEDED1F73F, 0xF7A255D83BC373F8, 0xD7F4F2448C0CEB81, 0xD95BE88CD210FFA7,
    0x336F52F8FF4728E7, 0xA74049DAC312AC71, 0xA2F61BB6E437FDB5, 0x4F2A5CB07F6A35B3,
    0x87D380BDA5BF7859, 0x16B9F7E06C453A21, 0x7BA2484C8A0FD54E, 0xF3A678CAD9A2E38C,
    0x39B0BF7DDE437BA2, 0xFCAF55C1BF8A4424, 0x18FCF680573FA594, 0x4C0563B89F495AC3,
    0x40E087931A00930D, 0x8CFFA9412EB642C1, 0x68CA39053261169F, 0x7A1EE967D27579E2,
    0x9D1D60E5076F5B6F, 0x3810E399B6F65BA2, 0x32095B6D4AB5F9B1, 0x35CAB62109DD038A,
    0xA90B24499FCFAFB1, 0x77A225A07CC2C6BD, 0x513E5E634C70E331, 0x4361C0CA3F692F12,
    0xD941ACA44B20A45B, 0x528F7C8602C5807B, 0x52AB92BEB9613989, 0x9D1DFA2EFC557F73,
    0x722FF175F572C348, 0x1D1260A51107FE97, 0x7A249A57EC0C9BA2, 0x04208FE9E8F7F2D6,
    0x5A110C6058B920A0, 0x0CD9A497658A5698, 0x56FD23C8F9715A4C, 0x284C847B9D887AAE,
    0x04FEABFBBDB619CB, 0x742E1E651C60BA83, 0x9A9632E65904AD3C, 0x881B82A13B51B9E2,
    0x506E6744CD974924, 0xB0183DB56FFC6A79, 0x0ED9B915C66ED37E, 0x5E11E86D5873D484,
    0xF678647E3519AC6E, 0x1B85D488D0F20CC5, 0xDAB9FE6525D89021, 0x0D151D86ADB73615,
    0xA865A54EDCC0F019, 0x93C42566AEF98FFB, 0x99E7AFEABE000731, 0x48CBFF086DDF285A,
    0x7F9B6AF1EBF78BAF, 0x58627E1A149BBA21, 0x2CD16E2ABD791E33, 0xD363EFF5F0977996,
    0x0CE2A38C344A6EED, 0x1A804AADB9CFA741, 0x907F30421D78C5DE, 0x501F65EDB3034D07,
    0x37624AE5A48FA6E9, 0x957BAF61700CFF4E, 0x3A6C27934E31188A, 0xD49503536ABCA345,
    0x088E049589C432E0, 0xF943AEE7FEBF21B8, 0x6C3B8E3E336139D3, 0x364F6FFA464EE52E,
    0xD60F6DCEDC314222, 0x56963B0DCA418FC0, 0x16F50EDF91E513AF, 0xEF1955914B609F93,
    0x565601C0364E3228, 0xECB53939887E8175, 0xBAC7A9A18531294B, 0xB344C470397BBA52,
    0x65D34954DAF3CEBD, 0xB4B81B3FA97511E2, 0xB422061193D6F6A7, 0x071582401C38434D,
    0x7A13F18BBEDC4FF5, 0xBC4097B116C524D2, 0x59B97885E2F2EA28, 0x99170A5DC3115544,
    0x6F423357E7C6A9F9, 0x325928EE6E6F8794, 0xD0E4366228B03343, 0x565C31F7DE89EA27,
    0x30F5611484119414, 0xD873DB391292ED4F, 0x7BD94E1D8E17DEBC, 0xC7D9F16864A76E94,
    0x947AE053EE56E63C, 0xC8C93882F9475F5F, 0x3A9BF55BA91F81CA, 0xD9A11FBB3D9808E4,
    0x0FD22063EDC29FCA, 0xB3F256D8ACA0B0B9, 0xB03031A8B4516E84, 0x35DD37D5871448AF,
    0xE9F6082B05542E4E, 0xEBFAFA33D7254B59, 0x9255ABB50D532280, 0xB9AB4CE57F2D34F3,
    0x693501D628297551, 0xC62C58F97DD949BF, 0xCD454F8F19C5126A, 0xBBE83F4ECC2BDECB,
    0xDC842B7E2819E230, 0xBA89142E007503B8, 0xA3BC941D0A5061CB, 0xE9F6760E32CD8021,
    0x09C7E552BC76492F, 0x852F54934DA55CC9, 0x8107FCCF064FCF56, 0x098954D51FFF6580,
    0x23B70EDB1955C4BF, 0xC330DE426430F69D, 0x4715ED43E8A45C0A, 0xA8D7E4DAB780A08D,
    0x0572B974F03CE0BB, 0xB57D2E985E1419C7, 0xE8D9ECBE2CF3D73F, 0x2FE4B17170E59750,
    0x11317BA87905E790, 0x7FBF21EC8A1F45EC, 0x1725CABFCB045B00, 0x964E915CD5E2B207,
    0x3E2B8BCBF016D66D, 0xBE7444E39328A0AC, 0xF85B2B4FBCDE44B7, 0x49353FEA39BA63B1,
    0x1DD01AAFCD53486A, 0x1FCA8A92FD719F85, 0xFC7C95D827357AFA, 0x18A6A990C8B35EBD,
    0xCCCB7005C6B9C28D, 0x3BDBB92C43B17F26, 0xAA70B5B4F89695A2, 0xE94C39A54A98307F,
    0xB7A0B174CFF6F36E, 0xD4DBA84729AF48AD, 0x2E18BC1AD9704A68, 0x2DE0966DAF2F8B1C,
    0xB9C11D5B1E43A07E, 0x64972D68DEE33360, 0x94628D38D0C20584, 0xDBC0D2B6AB90A559,
    0xD2733C4335C6A72F, 0x7E75D99D94A70F4D, 0x6CED1983376FA72B, 0x97FCAACBF030BC24,
    0x7B77497B32503B12, 0x8547EDDFB81CCB94, 0x79999CDFF70902CB, 0xCFFE1939438E9B24,
    0x829626E3892D95D7, 0x92FAE24291F2B3F1, 0x63E22C147B9C3403, 0xC678B6D860284A1C,
    0x5873888850659AE7, 0x0981DCD296A8736D, 0x9F65789A6509A440, 0x9FF38FED72E9052F,
    0xE479EE5B9930578C, 0xE7F28ECD2D49EECD, 0x56C074A581EA17FE, 0x5544F7D774B14AEF,
    0x7B3F0195FC6F290F, 0x12153635B2C0CF57, 0x7F5126DBBA5E0CA7, 0x7A76956C3EAFB413,
    0x3D5774A11D31AB39, 0x8A1B083821F40CB4, 0x7B4A38E32537DF62, 0x950113646D1D6E03,
    0x4DA8979A0041E8A9, 0x3BC36E078F7515D7, 0x5D0A12F27AD310D1, 0x7F9D1A2E1EBE1327,
    0xDA3A361B1C5157B1, 0xDCDD7D20903D0C25, 0x36833336D068F707, 0xCE68341F79893389,
    0xAB9090168DD05F34, 0x43954B3252DC25E5, 0xB438C2B67F98E5E9, 0x10DCD78E3851A492,
    0xDBC27AB5447822BF, 0x9B3CDB65F82CA382, 0xB67B7896167B4C84, 0xBFCED1B0048EAC50,
    0xA9119B60369FFEBD, 0x1FFF7AC80904BF45, 0xAC12FB171817EEE7, 0xAF08DA9177DDA93D,
    0x1B0CAB936E65C744, 0xB559EB1D04E5E932, 0xC37B45B3F8D6F2BA, 0xC3A9DC228CAAC9E9,
    0xF3B8B6675A6507FF, 0x9FC477DE4ED681DA, 0x67378D8ECCEF96CB, 0x6DD856D94D259236,
    0xA319CE15B0B4DB31, 0x073973751F12DD5E, 0x8A8E849EB32781A5, 0xE1925C71285279F5,
    0x74C04BF1790C0EFE, 0x4DDA48153C94938A, 0x9D266D6A1CC0542C, 0x7440FB816508C4FE,
    0x13328503DF48229F, 0xD6BF7BAEE43CAC40, 0x4838D65F6EF6748F, 0x1E152328F3318DEA,
    0x8F8419A348F296BF, 0x72C8834A5957B511, 0xD7A023A73260B45C, 0x94EBC8ABCFB56DAE,
    0x9FC10D0F989993E0, 0xDE68A2355B93CAE6, 0xA44CFE79AE538BBE, 0x9D1D84FCCE371425,
    0x51D2B1AB2DDFB636, 0x2FD7E4B9E72CD38C, 0x65CA5B96B7552210, 0xDD69A0D8AB3B546D,
    0x604D51B25FBF70E2, 0x73AA8A564FB7AC9E, 0x1A8C1E992B941148, 0xAAC40A2703D9BEA0,
    0x764DBEAE7FA4F3A6, 0x1E99B96E70A9BE8B, 0x2C5E9DEB57EF4743, 0x3A938FEE32D29981,
    0x26E6DB8FFDF5ADFE, 0x469356C504EC9F9D, 0xC8763C5B08D1908C, 0x3F6C6AF859D80055,
    0x7F7CC39420A3A545, 0x9BFB227EBDF4C5CE, 0x89039D79D6FC5C5C, 0x8FE88B57305E2AB6,
    0xA09E8C8C35AB96DE, 0xFA7E393983325753, 0xD6B6D0ECC617C699, 0xDFEA21EA9E7557E3,
    0xB67C1FA481680AF8, 0xCA1E3785A9E724E5, 0x1CFC8BED0D681639, 0xD18D8549D140CAEA,
    0x4ED0FE7E9DC91335, 0xE4DBF0634473F5D2, 0x1761F93A44D5AEFE, 0x53898E4C3910DA55,
    0x734DE8181F6EC39A, 0x2680B122BAA28D97, 0x298AF231C85BAFAB, 0x7983EED3740847D5,
    0x66C1A2A1A60CD889, 0x9E17E49642A3E4C1, 0xEDB454E7BADC0805, 0x50B704CAB602C329,
    0x4CC317FB9CDDD023, 0x66B4835D9EAFEA22, 0x219B97E26FFC81BD, 0x261E4E4C0A333A9D,
    0x1FE2CCA76517DB90, 0xD7504DFA8816EDBB, 0xB9571FA04DC089C8, 0x1DDC0325259B27DE,
    0xCF3F4688801EB9AA, 0xF4F5D05C10CAB243, 0x38B6525C21A42B0E, 0x36F60E2BA4FA6800,
    0xEB3593803173E0CE, 0x9C4CD6257C5A3603, 0xAF0C317D32ADAA8A, 0x258E5A80C7204C4B,
    0x8B889D624D44885D, 0xF4D14597E660F855, 0xD4347F66EC8941C3, 0xE699ED85B0DFB40D,
    0x2472F6207C2D0484, 0xC2A1E7B5B459AEB5, 0xAB4F6451CC1D45EC, 0x63767572AE3D6174,
    0xA59E0BD101731A28, 0x116D0016CB948F09, 0x2CF9C8CA052F6E9F, 0x0B090A7560A968E3,
    0xABEEDDB2DDE06FF1, 0x58EFC10B06A2068D, 0xC6E57A78FBD986E0, 0x2EAB8CA63CE802D7,
    0x14A195640116F336, 0x7C0828DD624EC390, 0xD74BBE77E6116AC7, 0x804456AF10F5FB53,
    0xEBE9EA2ADF4321C7, 0x03219A39EE587A30, 0x49787FEF17AF9924, 0xA1E9300CD8520548,
    0x5B45E522E4B1B4EF, 0xB49C3B3995091A36, 0xD4490AD526F14431, 0x12A8F216AF9418C2,
    0x001F837CC7350524, 0x1877B51E57A764D5, 0xA2853B80F17F58EE, 0x993E1DE72D36D310,
    0xB3598080CE64A656, 0x252F59CF0D9F04BB, 0xD23C8E176D113600, 0x1BDA0492E7E4586E,
    0x21E0BD5026C619BF, 0x3B097ADAF088F94E, 0x8D14DEDB30BE846E, 0xF95CFFA23AF5F6F4,
    0x3871700761B3F743, 0xCA672B91E9E4FA16, 0x64C8E531BFF53B55, 0x241260ED4AD1E87D,
    0x106C09B972D2E822, 0x7FBA195410E5CA30, 0x7884D9BC6CB569D8, 0x0647DFEDCD894A29,
    0x63573FF03E224774, 0x4FC8E9560F91B123, 0x1DB956E450275779, 0xB8D91274B9E9D4FB,
    0xA2EBEE47E2FBFCE1, 0xD9F1F30CCD97FB09, 0xEFED53D75FD64E6B, 0x2E6D02C36017F67F,
    0xA9AA4D20DB084E9B, 0xB64BE8D8B25396C1, 0x70CB6AF7C2D5BCF0, 0x98F076A4F7A2322E,
    0xBF84470805E69B5F, 0x94C3251F06F90CF3, 0x3E003E616A6591E9, 0xB925A6CD0421AFF3,
    0x61BDD1307C66E300, 0xBF8D5108E27E0D48, 0x240AB57A8B888B20, 0xFC87614BAF287E07,
    0xEF02CDD06FFDB432, 0xA1082C0466DF6C0A, 0x8215E577001332C8, 0xD39BB9C3A48DB6CF,
    0x2738259634305C14, 0x61CF4F94C97DF93D, 0x1B6BACA2AE4E125B, 0x758F450C88572E0B,
    0x959F587D507A8359, 0xB063E962E045F54D, 0x60E8ED72C0DFF5D1, 0x7B64978555326F9F,
    0xFD080D236DA814BA, 0x8C90FD9B083F4558, 0x106F72FE81E2C590, 0x7976033A39F7D952,
    0xA4EC0132764CA04B, 0x733EA705FAE4FA77, 0xB4D8F77BC3E56167, 0x9E21F4F903B33FD9,
    0x9D765E419FB69F6D, 0xD30C088BA61EA5EF, 0x5D94337FBFAF7F5B, 0x1A4E4822EB4D7A59,
    0x6FFE73E81B637FB3, 0xDDF957BC36D8B9CA, 0x64D0E29EEA8838B3, 0x08DD9BDFD96B9F63,
    0x087E79E5A57D1D13, 0xE328E230E3E2B3FB, 0x1C2559E30F0946BE, 0x720BF5F26F4D2EAA,
    0xB0774D261CC609DB, 0x443F64EC5A371195, 0x4112CF68649A260E, 0xD813F2FAB7F5C5CA,
    0x660D3257380841EE, 0x59AC2C7873F910A3, 0xE846963877671A17, 0x93B633ABFA3469F8,
    0xC0C0F5A60EF4CDCF, 0xCAF21ECD4377B28C, 0x57277707199B8175, 0x506C11B9D90E8B1D,
    0xD83CC2687A19255F, 0x4A29C6465A314CD1, 0xED2DF21216235097, 0xB5635C95FF7296E2,
    0x22AF003AB672E811, 0x52E762596BF68235, 0x9AEBA33AC6ECC6B0, 0x944F6DE09134DFB6,
    0x6C47BEC883A7DE39, 0x6AD047C430A12104, 0xA5B1CFDBA0AB4067, 0x7C45D833AFF07862,
    0x5092EF950A16DA0B, 0x9338E69C052B8E7B, 0x455A4B4CFE30E3F5, 0x6B02E63195AD0CF8,
    0x6B17B224BAD6BF27, 0xD1E0CCD25BB9C169, 0xDE0C89A556B9AE70, 0x50065E535A213CF6,
    0x9C1169FA2777B874, 0x78EDEFD694AF1EED, 0x6DC93D9526A50E68, 0xEE97F453F06791ED,
    0x32AB0EDB696703D3, 0x3A6853C7E70757A7, 0x31865CED6120F37D, 0x67FEF95D92607890,
    0x1F2B1D1F15F6DC9C, 0xB69E38A8965C6B65, 0xAA9119FF184CCCF4, 0xF43C732873F24C13,
    0xFB4A3D794A9A80D2, 0x3550C2321FD6109C, 0x371F77E76BB8417E, 0x6BFA9AAE5EC05779,
    0xCD04F3FF001A4778, 0xE3273522064480CA, 0x9F91508BFFCFC14A, 0x049A7F41061A9E60,
    0xFCB6BE43A9F2FE9B, 0x08DE8A1C7797DA9B, 0x8F9887E6078735A1, 0xB5B4071DBFC73A66,
    0x230E343DFBA08D33, 0x43ED7F5A0FAE657D, 0x3A88A0FBBCB05C63, 0x21874B8B4D2DBC4F,
    0x1BDEA12E35F6A8C9, 0x53C065C6C8E63528, 0xE34A1D250E7A8D6B, 0xD6B04D3B7651DD7E,
    0x5E90277E7CB39E2D, 0x2C046F22062DC67D, 0xB10BB459132D0A26, 0x3FA9DDFB67E2F199,
    0x0E09B88E1914F7AF, 0x10E8B35AF3EEAB37, 0x9EEDECA8E272B933, 0xD4C718BC4AE8AE5F,
    0x81536D601170FC20, 0x91B534F885818A06, 0xEC8177F83F900978, 0x190E714FADA5156E,
    0xB592BF39B0364963, 0x89C350C893AE7DC1, 0xAC042E70F8B383F2, 0xB49B52E587A1EE60,
    0xFB152FE3FF26DA89, 0x3E666E6F69AE2C15, 0x3B544EBE544C19F9, 0xE805A1E290CF2456,
    0x24B33C9D7ED25117, 0xE74733427B72F0C1, 0x0A804D18B7097475, 0x57E3306D881EDB4F,
    0x4AE7D6A36EB5DBCB, 0x2D8D5432157064C8, 0xD1E649DE1E7F268B, 0x8A328A1CEDFE552C,
    0x07A3AEC79624C7DA, 0x84547DDC3E203C94, 0x990A98FD5071D263, 0x1A4FF12616EEFC89,
    0xF6F7FD1431714200, 0x30C05B1BA332F41C, 0x8D2636B81555A786, 0x46C9FEB55D120902,
    0xCCEC0A73B49C9921, 0x4E9D2827355FC492, 0x19EBB029435DCB0F, 0x4659D2B743848A2C,
    0x963EF2C96B33BE31, 0x74F85198B05A2E7D, 0x5A0F544DD2B1FB18, 0x03727073C2E134B1,
    0xC7F6AA2DE59AEA61, 0x352787BAA0D7C22F, 0x9853EAB63B5E0B35, 0xABBDCDD7ED5C0860,
    0xCF05DAF5AC8D77B0, 0x49CAD48CEBF4A71E, 0x7A4C10EC2158C4A6, 0xD9E92AA246BF719E,
    0x13AE978D09FE5557, 0x730499AF921549FF, 0x4E4B705B92903BA4, 0xFF577222C14F0A3A,
    0x55B6344CF97AAFAE, 0xB862225B055B6960, 0xCAC09AFBDDD2CDB4, 0xDAF8E9829FE96B5F,
    0xB5FDFC5D3132C498, 0x310CB380DB6F7503, 0xE87FBB46217A360E, 0x2102AE466EBB1148,
    0xF8549E1A3AA5E00D, 0x07A69AFDCC42261A, 0xC4C118BFE78FEAAE, 0xF9F4892ED96BD438,
    0x1AF3DBE25D8F45DA, 0xF5B4B0B0D2DEEEB4, 0x962ACEEFA82E1C84, 0x046E3ECAAF453CE9,
    0xF05D129681949A4C, 0x964781CE734B3C84, 0x9C2ED44081CE5FBD, 0x522E23F3925E319E,
    0x177E00F9FC32F791, 0x2BC60A63A6F3B3F2, 0x222BBFAE61725606, 0x486289DDCC3D6780,
    0x7DC7785B8EFDFC80, 0x8AF38731C02BA980, 0x1FAB64EA29A2DDF7, 0xE4D9429322CD065A,
    0x9DA058C67844F20C, 0x24C0E332B70019B0, 0x233003B5A6CFE6AD, 0xD586BD01C5C217F6,
    0x5E5637885F29BC2B, 0x7EBA726D8C94094B, 0x0A56A5F0BFE39272, 0xD79476A84EE20D06,
    0x9E4C1269BAA4BF37, 0x17EFEE45B0DEE640, 0x1D95B0A5FCF90BC6, 0x93CBE0B699C2585D,
    0x65FA4F227A2B6D79, 0xD5F9E858292504D5, 0xC2B5A03F71471A6F, 0x59300222B4561E00,
    0xCE2F8642CA0712DC, 0x7CA9723FBB2E8988, 0x2785338347F2BA08, 0xC61BB3A141E50E8C,
    0x150F361DAB9DEC26, 0x9F6A419D382595F4, 0x64A53DC924FE7AC9, 0x142DE49FFF7A7C3D,
    0x0C335248857FA9E7, 0x0A9C32D5EAE45305, 0xE6C42178C4BBB92E, 0x71F1CE2490D20B07,
    0xF1BCC3D275AFE51A, 0xE728E8C83C334074, 0x96FBF83A12884624, 0x81A1549FD6573DA5,
    0x5FA7867CAF35E149, 0x56986E2EF3ED091B, 0x917F1DD5F8886C61, 0xD20D8C88C8FFE65F,
    0x31D71DCE64B2C310, 0xF165B587DF898190, 0xA57E6339DD2CF3A0, 0x1EF6E6DBB1961EC9,
    0x70CC73D90BC26E24, 0xE21A6B35DF0C3AD7, 0x003A93D8B2806962, 0x1C99DED33CB890A1,
    0xCF3145DE0ADD4289, 0xD0E4427A5514FB72, 0x77C621CC9FB3A483, 0x67A34DAC4356550B,
    0xF8D626AAAF278509};

static constexpr U64 castlingKey[16] = {
    0,
    RANDOM_ARRAY[768],
    RANDOM_ARRAY[768 + 1],
    RANDOM_ARRAY[768] ^ RANDOM_ARRAY[768 + 1],
    RANDOM_ARRAY[768 + 2],
    RANDOM_ARRAY[768] ^ RANDOM_ARRAY[768 + 2],
    RANDOM_ARRAY[768 + 1] ^ RANDOM_ARRAY[768 + 2],
    RANDOM_ARRAY[768] ^ RANDOM_ARRAY[768 + 1] ^ RANDOM_ARRAY[768 + 2],
    RANDOM_ARRAY[768 + 3],
    RANDOM_ARRAY[768] ^ RANDOM_ARRAY[768 + 3],
    RANDOM_ARRAY[768 + 1] ^ RANDOM_ARRAY[768 + 3],
    RANDOM_ARRAY[768] ^ RANDOM_ARRAY[768 + 1] ^ RANDOM_ARRAY[768 + 3],
    RANDOM_ARRAY[768 + 3] ^ RANDOM_ARRAY[768 + 2],
    RANDOM_ARRAY[768 + 3] ^ RANDOM_ARRAY[768 + 2] ^ RANDOM_ARRAY[768],
    RANDOM_ARRAY[768 + 1] ^ RANDOM_ARRAY[768 + 2] ^ RANDOM_ARRAY[768 + 3],
    RANDOM_ARRAY[768 + 1] ^ RANDOM_ARRAY[768 + 2] ^ RANDOM_ARRAY[768 + 3] ^ RANDOM_ARRAY[768]};

static constexpr int MAP_HASH_PIECE[12] = {1, 3, 5, 7, 9, 11, 0, 2, 4, 6, 8, 10};

/// @brief [Internal Usage]
/// @param piece
/// @param square
/// @return
inline U64 piece(Piece piece, Square square) {
    return RANDOM_ARRAY[64 * MAP_HASH_PIECE[static_cast<int>(piece)] + square];
}

/// @brief [Internal Usage]
/// @param file
/// @return
inline U64 enpassant(File file) { return RANDOM_ARRAY[772 + static_cast<int>(file)]; }

/// @brief [Internal Usage]
/// @param castling
/// @return
inline U64 castling(int castling) { return castlingKey[castling]; }

/// @brief [Internal Usage]
/// @param idx
/// @return
inline U64 castlingIndex(int idx) { return RANDOM_ARRAY[768 + idx]; }

inline U64 sideToMove() { return RANDOM_ARRAY[780]; }

}  // namespace zobrist

/****************************************************************************\
 * Forward declarations                                                      *
\****************************************************************************/

namespace movegen {

/// @brief Generates all legal moves for a position. The movelist will be
/// emptied before adding the moves.
/// @tparam mt
/// @param movelist
/// @param board
template <MoveGenType mt = MoveGenType::ALL>
void legalmoves(Movelist &movelist, const Board &board);

}  // namespace movegen

/****************************************************************************\
 * Attacks                                                                   *
\****************************************************************************/

namespace attacks {
Bitboard pawn(Color c, Square sq);
Bitboard knight(Square sq);
Bitboard bishop(Square sq, Bitboard occupied);
Bitboard rook(Square sq, Bitboard occupied);
Bitboard queen(Square sq, Bitboard occupied);
Bitboard king(Square sq);

template <Color c>
[[nodiscard]] Bitboard pawnLeftAttacks(const Bitboard pawns);

template <Color c>
[[nodiscard]] Bitboard pawnRightAttacks(const Bitboard pawns);

template <Direction direction>
[[nodiscard]] constexpr Bitboard shift(const Bitboard b);

static constexpr Bitboard MASK_RANK[8] = {
    0xff,         0xff00,         0xff0000,         0xff000000,
    0xff00000000, 0xff0000000000, 0xff000000000000, 0xff00000000000000};

static constexpr Bitboard MASK_FILE[8] = {
    0x101010101010101,  0x202020202020202,  0x404040404040404,  0x808080808080808,
    0x1010101010101010, 0x2020202020202020, 0x4040404040404040, 0x8080808080808080,
};

struct Magic {
    Bitboard mask;
    U64 magic;
    U64 *attacks;
    U64 shift;

    U64 operator()(U64 b) const { return ((b & mask) * magic) >> shift; }
};

constexpr Bitboard RookMagics[MAX_SQ] = {
    0x8a80104000800020ULL, 0x140002000100040ULL,  0x2801880a0017001ULL,  0x100081001000420ULL,
    0x200020010080420ULL,  0x3001c0002010008ULL,  0x8480008002000100ULL, 0x2080088004402900ULL,
    0x800098204000ULL,     0x2024401000200040ULL, 0x100802000801000ULL,  0x120800800801000ULL,
    0x208808088000400ULL,  0x2802200800400ULL,    0x2200800100020080ULL, 0x801000060821100ULL,
    0x80044006422000ULL,   0x100808020004000ULL,  0x12108a0010204200ULL, 0x140848010000802ULL,
    0x481828014002800ULL,  0x8094004002004100ULL, 0x4010040010010802ULL, 0x20008806104ULL,
    0x100400080208000ULL,  0x2040002120081000ULL, 0x21200680100081ULL,   0x20100080080080ULL,
    0x2000a00200410ULL,    0x20080800400ULL,      0x80088400100102ULL,   0x80004600042881ULL,
    0x4040008040800020ULL, 0x440003000200801ULL,  0x4200011004500ULL,    0x188020010100100ULL,
    0x14800401802800ULL,   0x2080040080800200ULL, 0x124080204001001ULL,  0x200046502000484ULL,
    0x480400080088020ULL,  0x1000422010034000ULL, 0x30200100110040ULL,   0x100021010009ULL,
    0x2002080100110004ULL, 0x202008004008002ULL,  0x20020004010100ULL,   0x2048440040820001ULL,
    0x101002200408200ULL,  0x40802000401080ULL,   0x4008142004410100ULL, 0x2060820c0120200ULL,
    0x1001004080100ULL,    0x20c020080040080ULL,  0x2935610830022400ULL, 0x44440041009200ULL,
    0x280001040802101ULL,  0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
    0x20030a0244872ULL,    0x12001008414402ULL,   0x2006104900a0804ULL,  0x1004081002402ULL};

constexpr Bitboard BishopMagics[MAX_SQ] = {
    0x40040844404084ULL,   0x2004208a004208ULL,   0x10190041080202ULL,   0x108060845042010ULL,
    0x581104180800210ULL,  0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050404440404ULL,    0x21001420088ULL,      0x24d0080801082102ULL, 0x1020a0a020400ULL,
    0x40308200402ULL,      0x4011002100800ULL,    0x401484104104005ULL,  0x801010402020200ULL,
    0x400210c3880100ULL,   0x404022024108200ULL,  0x810018200204102ULL,  0x4002801a02003ULL,
    0x85040820080400ULL,   0x810102c808880400ULL, 0xe900410884800ULL,    0x8002020480840102ULL,
    0x220200865090201ULL,  0x2010100a02021202ULL, 0x152048408022401ULL,  0x20080002081110ULL,
    0x4001001021004000ULL, 0x800040400a011002ULL, 0xe4004081011002ULL,   0x1c004001012080ULL,
    0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
    0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
    0x209188240001000ULL,  0x400408a884001800ULL, 0x110400a6080400ULL,   0x1840060a44020800ULL,
    0x90080104000041ULL,   0x201011000808101ULL,  0x1a2208080504f080ULL, 0x8012020600211212ULL,
    0x500861011240000ULL,  0x180806108200800ULL,  0x4000020e01040044ULL, 0x300000261044000aULL,
    0x802241102020002ULL,  0x20906061210001ULL,   0x5a84841004010310ULL, 0x4010801011c04ULL,
    0xa010109502200ULL,    0x4a02012000ULL,       0x500201010098b028ULL, 0x8040002811040900ULL,
    0x28000010020204ULL,   0x6000020202d0240ULL,  0x8918844842082200ULL, 0x4010011029020020ULL};

inline Bitboard RookAttacks[0x19000]  = {};
inline Bitboard BishopAttacks[0x1480] = {};

inline Magic RookTable[MAX_SQ]   = {};
inline Magic BishopTable[MAX_SQ] = {};

/// @brief
/// @param r
/// @param f
/// @return
[[nodiscard]] inline int validSq(Rank r, File f) {
    return r >= Rank::RANK_1 && r <= Rank::RANK_8 && f >= File::FILE_A && f <= File::FILE_H;
}

/// @brief Make a square from a rank and file
/// @param r
/// @param f
/// @return
[[nodiscard]] inline Square makeSquare(Rank r, File f) {
    return static_cast<Square>(int(r) * 8 + int(f));
}

namespace runtime {
/// @brief [Internal Usage] Slow function to calculate bishop attacks
/// @param sq
/// @param occupied
/// @return
[[nodiscard]] inline Bitboard bishopAttacks(Square sq, Bitboard occupied) {
    Bitboard attacks = 0ULL;

    int r, f;

    int br = sq / 8;
    int bf = sq % 8;

    for (r = br + 1, f = bf + 1; validSq(static_cast<Rank>(r), static_cast<File>(f)); r++, f++) {
        Square s = makeSquare(static_cast<Rank>(r), static_cast<File>(f));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    for (r = br - 1, f = bf + 1; validSq(static_cast<Rank>(r), static_cast<File>(f)); r--, f++) {
        Square s = makeSquare(static_cast<Rank>(r), static_cast<File>(f));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    for (r = br + 1, f = bf - 1; validSq(static_cast<Rank>(r), static_cast<File>(f)); r++, f--) {
        Square s = makeSquare(static_cast<Rank>(r), static_cast<File>(f));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    for (r = br - 1, f = bf - 1; validSq(static_cast<Rank>(r), static_cast<File>(f)); r--, f--) {
        Square s = makeSquare(static_cast<Rank>(r), static_cast<File>(f));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    return attacks;
}

/// @brief [Internal Usage] Slow function to calculate rook attacks
/// @param sq
/// @param occupied
/// @return
[[nodiscard]] inline Bitboard rookAttacks(Square sq, Bitboard occupied) {
    Bitboard attacks = 0ULL;

    int r, f;

    int rr = sq / 8;
    int rf = sq % 8;

    for (r = rr + 1; validSq(static_cast<Rank>(r), static_cast<File>(rf)); r++) {
        Square s = makeSquare(static_cast<Rank>(r), static_cast<File>(rf));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    for (r = rr - 1; validSq(static_cast<Rank>(r), static_cast<File>(rf)); r--) {
        Square s = makeSquare(static_cast<Rank>(r), static_cast<File>(rf));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    for (f = rf + 1; validSq(static_cast<Rank>(rr), static_cast<File>(f)); f++) {
        Square s = makeSquare(static_cast<Rank>(rr), static_cast<File>(f));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    for (f = rf - 1; validSq(static_cast<Rank>(rr), static_cast<File>(f)); f--) {
        Square s = makeSquare(static_cast<Rank>(rr), static_cast<File>(f));
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s)) break;
    }

    return attacks;
}

}  // namespace runtime

/// @brief [Internal Usage] Initializes the magic bitboard tables for sliding pieces
/// @param sq
/// @param table
/// @param magic
/// @param attacks
inline void initSliders(Square sq, Magic table[], U64 magic,
                        const std::function<Bitboard(Square, Bitboard)> &attacks) {
    const Bitboard edges =
        ((MASK_RANK[static_cast<int>(Rank::RANK_1)] | MASK_RANK[static_cast<int>(Rank::RANK_8)]) &
         ~MASK_RANK[static_cast<int>(utils::squareRank(sq))]) |
        ((MASK_FILE[static_cast<int>(File::FILE_A)] | MASK_FILE[static_cast<int>(File::FILE_H)]) &
         ~MASK_FILE[static_cast<int>(utils::squareFile(sq))]);

    Bitboard occ = 0ULL;

    table[sq].magic = magic;
    table[sq].mask  = attacks(sq, occ) & ~edges;
    table[sq].shift = MAX_SQ - builtin::popcount(table[sq].mask);

    if (sq < MAX_SQ - 1) {
        table[sq + 1].attacks = table[sq].attacks + (1 << builtin::popcount(table[sq].mask));
    }

    do {
        table[sq].attacks[table[sq](occ)] = attacks(sq, occ);
        occ                               = (occ - table[sq].mask) & table[sq].mask;
    } while (occ);
}

/// @brief [Internal Usage] Initializes the attacks for the bishop and rook. Called once at startup.
inline void initAttacks() {
    BishopTable[0].attacks = BishopAttacks;
    RookTable[0].attacks   = RookAttacks;

    for (int i = 0; i < MAX_SQ; i++) {
        initSliders(static_cast<Square>(i), BishopTable, BishopMagics[i], runtime::bishopAttacks);
        initSliders(static_cast<Square>(i), RookTable, RookMagics[i], runtime::rookAttacks);
    }
}

// force initialization of attacks
static auto init = []() {
    initAttacks();
    return 0;
}();

}  // namespace attacks

/****************************************************************************\
 * Board                                                                     *
\****************************************************************************/
class Board {
   public:
    explicit Board(std::string fen = STARTPOS);

    /// @brief [Internal Usage]
    /// @param fen
    void setFenInternal(std::string fen);

    virtual void setFen(const std::string &fen);

    /// @brief Get the current FEN string.
    /// @return
    [[nodiscard]] std::string getFen() const;

    void makeMove(const Move &move);
    void unmakeMove(const Move &move);

    /// @brief Make a null move. (Switches the side to move)
    void makeNullMove();
    /// @brief Unmake a null move. (Switches the side to move)
    void unmakeNullMove();

    /// @brief Get the occupancy bitboard from us.
    /// @param color
    /// @return
    [[nodiscard]] Bitboard us(Color color) const {
        return pieces(PieceType::PAWN, color) | pieces(PieceType::KNIGHT, color) |
               pieces(PieceType::BISHOP, color) | pieces(PieceType::ROOK, color) |
               pieces(PieceType::QUEEN, color) | pieces(PieceType::KING, color);
    }

    /// @brief Get the occupancy bitboard of the enemy.
    /// @param color
    /// @return
    [[nodiscard]] Bitboard them(Color color) const { return us(~color); }

    /// @brief Get the current occupancy bitboard.
    /// Faster than calling all() or
    /// us(board.sideToMove()) | them(board.sideToMove()).
    /// @return
    [[nodiscard]] Bitboard occ() const {
        assert(occ_all_ == all());
        return occ_all_;
    }

    /// @brief recalculate all bitboards
    /// @return
    [[nodiscard]] Bitboard all() const { return us(Color::WHITE) | us(Color::BLACK); }

    /// @brief Returns the square of the king for a certain color
    /// @param color
    /// @return
    [[nodiscard]] Square kingSq(Color color) const {
        assert(pieces(PieceType::KING, color) != 0);
        return builtin::lsb(pieces(PieceType::KING, color));
    }

    /// @brief Returns all pieces of a certain type and color
    /// @param type
    /// @param color
    /// @return
    [[nodiscard]] Bitboard pieces(PieceType type, Color color) const {
        return pieces_bb_[static_cast<int>(color)][static_cast<int>(type)];
    }

    /// @brief Returns all pieces of a certain type
    /// @param type
    /// @return
    [[nodiscard]] Bitboard pieces(PieceType type) const {
        return pieces(type, Color::WHITE) | pieces(type, Color::BLACK);
    }

    /// @brief Returns either the piece or the piece type on a square
    /// @tparam T
    /// @param sq
    /// @return
    template <typename T = Piece>
    [[nodiscard]] T at(Square sq) const {
        if constexpr (std::is_same_v<T, PieceType>) {
            return utils::typeOfPiece(board_[sq]);
        } else {
            return board_[sq];
        }
    }

    /// @brief Checks if a move is a capture, enpassant moves are also considered captures.
    /// @param move
    /// @return
    bool isCapture(const Move &move) const {
        return (at(move.to()) != Piece::NONE && move.typeOf() != Move::CASTLING) ||
               move.typeOf() == Move::ENPASSANT;
    }

    [[nodiscard]] static Color color(Piece piece) {
        return static_cast<Color>(static_cast<int>(piece) / 6);
    }

    /// @brief Get the current hash key of the board
    /// @return
    [[nodiscard]] U64 hash() const { return hash_key_; }
    [[nodiscard]] Color sideToMove() const { return side_to_move_; }
    [[nodiscard]] Square enpassantSq() const { return enpassant_sq_; }
    [[nodiscard]] CastlingRights castlingRights() const { return castling_rights_; }
    [[nodiscard]] int halfMoveClock() const { return half_moves_; }
    [[nodiscard]] int fullMoveNumber() const { return full_moves_; }

    void set960(bool is960) {
        chess960_ = is960;
        setFen(original_fen_);
    }

    /// @brief Checks if the current position is a chess960, aka. FRC/DFRC position.
    /// @return
    [[nodiscard]] bool chess960() const { return chess960_; }

    /// @brief Get the castling rights as a string
    /// @return
    [[nodiscard]] std::string getCastleString() const;

    /// @brief Checks if the current position is a repetition, set this to 1 if you are writing
    /// a chess engine.
    /// @param count
    /// @return
    [[nodiscard]] bool isRepetition(int count = 2) const;

    /// @brief Checks if the current position is a draw by 50 move rule.
    /// @return
    [[nodiscard]] bool isHalfMoveDraw() const { return half_moves_ >= 100; }

    /// @brief Checks if the current position is a draw by insufficient material.
    /// @return
    [[nodiscard]] bool isInsufficientMaterial() const;

    /// @brief Checks if the game is over. Returns GameResultReason::NONE if the game is not over.
    /// This function calculates all legal moves for the current position to check if the game is
    /// over. If you are writing you should not use this function.
    /// @return
    [[nodiscard]] std::pair<GameResultReason, GameResult> isGameOver() const;

    /// @brief Checks if a square is attacked by the given color.
    /// @param square
    /// @param color
    /// @return
    [[nodiscard]] bool isAttacked(Square square, Color color) const;

    /// @brief Checks if the current side to move is in check
    /// @return
    [[nodiscard]] bool inCheck() const;

    /// @brief Regenerates the zobrist hash key
    /// @return
    [[nodiscard]] U64 zobrist() const;

    friend std::ostream &operator<<(std::ostream &os, const Board &board);

   protected:
    virtual void placePiece(Piece piece, Square sq);
    virtual void removePiece(Piece piece, Square sq);

    std::vector<State> prev_states_;

    U64 pieces_bb_[2][6]         = {};
    std::array<Piece, 64> board_ = {};

    U64 hash_key_ = 0ULL;
    U64 occ_all_  = 0ULL;

    CastlingRights castling_rights_ = {};
    uint16_t full_moves_            = 1;

    Color side_to_move_  = Color::WHITE;
    Square enpassant_sq_ = Square::NO_SQ;
    uint8_t half_moves_  = 0;

    bool chess960_ = false;

   private:
    std::string original_fen_;
};

/****************************************************************************\
 * Board Implementations                                                     *
\****************************************************************************/
inline Board::Board(std::string fen) { setFenInternal(std::move(fen)); }

inline void Board::setFenInternal(std::string fen) {
    original_fen_ = fen;

    std::fill(std::begin(board_), std::end(board_), Piece::NONE);

    utils::trim(fen);

    occ_all_ = 0ULL;

    for (const auto c : {Color::WHITE, Color::BLACK}) {
        for (int i = 0; i < 6; i++) {
            pieces_bb_[static_cast<int>(c)][i] = 0ULL;
        }
    }

    const std::vector<std::string> params = utils::splitString(fen, ' ');

    const std::string &position   = params[0];
    const std::string &move_right = params[1];
    const std::string &castling   = params[2];
    const std::string &en_passant = params[3];

    half_moves_ = std::stoi(params.size() > 4 ? params[4] : "0");
    full_moves_ = std::stoi(params.size() > 4 ? params[5] : "1") * 2;

    side_to_move_ = (move_right == "w") ? Color::WHITE : Color::BLACK;

    auto square = Square(56);
    for (char curr : position) {
        if (charToPiece(curr) != Piece::NONE) {
            const Piece piece = charToPiece(curr);
            placePiece(piece, square);
            hash_key_ ^= zobrist::piece(piece, square);

            square = Square(square + 1);
        } else if (curr == '/')
            square = Square(square - 16);
        else if (isdigit(curr)) {
            square = Square(square + (curr - '0'));
        }
    }

    castling_rights_.clearAllCastlingRights();

    for (char i : castling) {
        if (i == '-') break;
        if (!chess960_) {
            if (i == 'K')
                castling_rights_
                    .setCastlingRight<Color::WHITE, CastleSide::KING_SIDE, File::FILE_H>();
            if (i == 'Q')
                castling_rights_
                    .setCastlingRight<Color::WHITE, CastleSide::QUEEN_SIDE, File::FILE_A>();
            if (i == 'k')
                castling_rights_
                    .setCastlingRight<Color::BLACK, CastleSide::KING_SIDE, File::FILE_H>();
            if (i == 'q')
                castling_rights_
                    .setCastlingRight<Color::BLACK, CastleSide::QUEEN_SIDE, File::FILE_A>();
        } else {
            if (i == 'K' || i == 'k') {
                // find rook on the right side of the king
                const auto color   = isupper(i) ? Color::WHITE : Color::BLACK;
                const auto king_sq = kingSq(color);

                const auto sq_corner = color == Color::WHITE ? SQ_H1 : SQ_H8;

                for (int sq = king_sq + 1; sq <= sq_corner; sq++) {
                    if (at<PieceType>(Square(sq)) == PieceType::NONE) continue;
                    if (at<PieceType>(Square(sq)) == PieceType::ROOK &&
                        int(at(Square(sq))) / 6 == int(color)) {
                        castling_rights_.setCastlingRight(color, CastleSide::KING_SIDE,
                                                          utils::squareFile(Square(sq)));
                        break;
                    }
                }

            } else if (i == 'Q' || i == 'q') {
                // find rook on the left side of the king
                const auto color   = isupper(i) ? Color::WHITE : Color::BLACK;
                const auto king_sq = kingSq(color);

                const auto sq_corner = color == Color::WHITE ? SQ_A1 : SQ_A8;

                for (int sq = king_sq - 1; sq >= sq_corner; sq--) {
                    if (at<PieceType>(Square(sq)) == PieceType::NONE) continue;
                    if (at<PieceType>(Square(sq)) == PieceType::ROOK &&
                        int(at(Square(sq))) / 6 == int(color)) {
                        castling_rights_.setCastlingRight(color, CastleSide::QUEEN_SIDE,
                                                          utils::squareFile(Square(sq)));
                        break;
                    }
                }
            } else {
                const auto color   = isupper(i) ? Color::WHITE : Color::BLACK;
                const auto king_sq = kingSq(color);
                const auto file    = static_cast<File>(tolower(i) - 97);
                const auto side    = int(file) > int(utils::squareFile(king_sq))
                                         ? CastleSide::KING_SIDE
                                         : CastleSide::QUEEN_SIDE;
                castling_rights_.setCastlingRight(color, side, file);
            }
        }
    }

    if (en_passant == "-") {
        enpassant_sq_ = NO_SQ;
    } else {
        const char letter = en_passant[0];
        const int file    = letter - 96;
        const int rank    = en_passant[1] - 48;
        enpassant_sq_     = Square((rank - 1) * 8 + file - 1);
    }

    hash_key_ = zobrist();
    occ_all_  = all();

    prev_states_.clear();
    prev_states_.reserve(150);
}

inline void Board::setFen(const std::string &fen) { setFenInternal(fen); }

[[nodiscard]] inline std::string Board::getFen() const {
    std::stringstream ss;

    // Loop through the ranks of the board in reverse order
    for (int rank = 7; rank >= 0; rank--) {
        int free_space = 0;

        // Loop through the files of the board
        for (int file = 0; file < 8; file++) {
            // Calculate the square index
            const int sq = rank * 8 + file;

            // Get the piece at the current square
            const Piece piece = at(Square(sq));

            // If there is a piece at the current square
            if (piece != Piece::NONE) {
                // If there were any empty squares before this piece,
                // append the number of empty squares to the FEN string
                if (free_space) {
                    ss << free_space;
                    free_space = 0;
                }

                // Append the character representing the piece to the FEN string
                ss << pieceToChar(piece);
            } else {
                // If there is no piece at the current square, increment the
                // counter for the number of empty squares
                free_space++;
            }
        }

        // If there are any empty squares at the end of the rank,
        // append the number of empty squares to the FEN string
        if (free_space != 0) {
            ss << free_space;
        }

        // Append a "/" character to the FEN string, unless this is the last rank
        ss << (rank > 0 ? "/" : "");
    }

    // Append " w " or " b " to the FEN string, depending on which player's turn it is
    ss << (side_to_move_ == Color::WHITE ? " w " : " b ");

    // Append the appropriate characters to the FEN string to indicate
    // whether castling is allowed for each player
    ss << getCastleString();
    if (castling_rights_.isEmpty()) ss << "-";

    // Append information about the en passant square (if any)
    // and the half-move clock and full move number to the FEN string
    if (enpassant_sq_ == NO_SQ)
        ss << " - ";
    else
        ss << " " << squareToString[enpassant_sq_] << " ";

    ss << int(half_moves_) << " " << int(full_moves_ / 2);

    // Return the resulting FEN string
    return ss.str();
}

[[nodiscard]] inline U64 Board::zobrist() const {
    U64 hash_key = 0ULL;

    U64 wPieces = us(Color::WHITE);
    U64 bPieces = us(Color::BLACK);

    while (wPieces) {
        const Square sq = builtin::poplsb(wPieces);
        hash_key ^= zobrist::piece(at(sq), sq);
    }
    while (bPieces) {
        const Square sq = builtin::poplsb(bPieces);
        hash_key ^= zobrist::piece(at(sq), sq);
    }

    U64 ep_hash = 0ULL;
    if (enpassant_sq_ != NO_SQ) ep_hash ^= zobrist::enpassant(utils::squareFile(enpassant_sq_));

    U64 side_to_move_hash = 0ULL;
    if (side_to_move_ == Color::WHITE) side_to_move_hash ^= zobrist::sideToMove();

    // Castle hash

    U64 castling_hash = 0ULL;
    castling_hash ^= zobrist::castling(castling_rights_.getHashIndex());

    return hash_key ^ ep_hash ^ side_to_move_hash ^ castling_hash;
}

inline std::ostream &operator<<(std::ostream &os, const Board &b) {
    for (int i = 63; i >= 0; i -= 8) {
        os << " " << pieceToChar(b.board_[i - 7]) << " " << pieceToChar(b.board_[i - 6]) << " "
           << pieceToChar(b.board_[i - 5]) << " " << pieceToChar(b.board_[i - 4]) << " "
           << pieceToChar(b.board_[i - 3]) << " " << pieceToChar(b.board_[i - 2]) << " "
           << pieceToChar(b.board_[i - 1]) << " " << pieceToChar(b.board_[i]) << " \n";
    }
    os << "\n\n";
    os << "Side to move: " << static_cast<int>(b.side_to_move_) << "\n";
    os << "Castling rights: " << b.getCastleString() << "\n";
    os << "Halfmoves: " << static_cast<int>(b.half_moves_) << "\n";
    os << "Fullmoves: " << static_cast<int>(b.full_moves_) / 2 << "\n";
    os << "EP: " << static_cast<int>(b.enpassant_sq_) << "\n";
    os << "Hash: " << b.hash_key_ << "\n";

    os << std::endl;
    return os;
}

inline std::string Board::getCastleString() const {
    std::stringstream ss;

    if (chess960_) {
        if (castling_rights_.hasCastlingRight(Color::WHITE, CastleSide::KING_SIDE))
            ss << char(char(castling_rights_.getRookFile(Color::WHITE, CastleSide::KING_SIDE)) +
                       65);
        if (castling_rights_.hasCastlingRight(Color::WHITE, CastleSide::QUEEN_SIDE))
            ss << char(char(castling_rights_.getRookFile(Color::WHITE, CastleSide::QUEEN_SIDE)) +
                       65);
        if (castling_rights_.hasCastlingRight(Color::BLACK, CastleSide::KING_SIDE))
            ss << char(char(castling_rights_.getRookFile(Color::BLACK, CastleSide::KING_SIDE)) +
                       97);
        if (castling_rights_.hasCastlingRight(Color::BLACK, CastleSide::QUEEN_SIDE))
            ss << char(char(castling_rights_.getRookFile(Color::BLACK, CastleSide::QUEEN_SIDE)) +
                       97);
    } else {
        if (castling_rights_.hasCastlingRight(Color::WHITE, CastleSide::KING_SIDE)) ss << "K";
        if (castling_rights_.hasCastlingRight(Color::WHITE, CastleSide::QUEEN_SIDE)) ss << "Q";
        if (castling_rights_.hasCastlingRight(Color::BLACK, CastleSide::KING_SIDE)) ss << "k";
        if (castling_rights_.hasCastlingRight(Color::BLACK, CastleSide::QUEEN_SIDE)) ss << "q";
    }

    return ss.str();
}

inline bool Board::isRepetition(int count) const {
    uint8_t c = 0;

    for (int i = static_cast<int>(prev_states_.size()) - 2;
         i >= 0 && i >= static_cast<int>(prev_states_.size()) - half_moves_ - 1; i -= 2) {
        if (prev_states_[i].hash == hash_key_) c++;

        if (c == count) return true;
    }

    return false;
}

inline bool Board::isInsufficientMaterial() const {
    const auto count = builtin::popcount(occ());

    if (count == 2) return true;

    if (count == 3) {
        if (pieces(PieceType::BISHOP, Color::WHITE) || pieces(PieceType::BISHOP, Color::BLACK))
            return true;
        if (pieces(PieceType::KNIGHT, Color::WHITE) || pieces(PieceType::KNIGHT, Color::BLACK))
            return true;
    }

    if (count == 4) {
        if (pieces(PieceType::BISHOP, Color::WHITE) && pieces(PieceType::BISHOP, Color::BLACK) &&
            utils::sameColor(builtin::lsb(pieces(PieceType::BISHOP, Color::WHITE)),
                             builtin::lsb(pieces(PieceType::BISHOP, Color::BLACK))))
            return true;
    }

    return false;
}

inline std::pair<GameResultReason, GameResult> Board::isGameOver() const {
    if (isHalfMoveDraw()) {
        const Board &board = *this;

        Movelist movelist;
        movegen::legalmoves<MoveGenType::ALL>(movelist, board);

        if (movelist.empty() && inCheck()) {
            return {GameResultReason::CHECKMATE, GameResult::LOSE};
        }

        return {GameResultReason::FIFTY_MOVE_RULE, GameResult::DRAW};
    }

    if (isInsufficientMaterial())
        return {GameResultReason::INSUFFICIENT_MATERIAL, GameResult::DRAW};

    if (isRepetition()) return {GameResultReason::THREEFOLD_REPETITION, GameResult::DRAW};

    const Board &board = *this;

    Movelist movelist;
    movegen::legalmoves<MoveGenType::ALL>(movelist, board);

    if (movelist.empty()) {
        if (inCheck()) return {GameResultReason::CHECKMATE, GameResult::LOSE};
        return {GameResultReason::STALEMATE, GameResult::DRAW};
    }

    return {GameResultReason::NONE, GameResult::NONE};
}

inline bool Board::isAttacked(Square square, Color color) const {
    if (attacks::pawn(~color, square) & pieces(PieceType::PAWN, color)) return true;
    if (attacks::knight(square) & pieces(PieceType::KNIGHT, color)) return true;
    if (attacks::king(square) & pieces(PieceType::KING, color)) return true;

    if (attacks::bishop(square, occ()) &
        (pieces(PieceType::BISHOP, color) | pieces(PieceType::QUEEN, color)))
        return true;
    if (attacks::rook(square, occ()) &
        (pieces(PieceType::ROOK, color) | pieces(PieceType::QUEEN, color)))
        return true;
    return false;
}

inline bool Board::inCheck() const { return isAttacked(kingSq(side_to_move_), ~side_to_move_); }

inline void Board::placePiece(Piece piece, Square sq) {
    assert(board_[sq] == Piece::NONE);

    pieces_bb_[static_cast<int>(color(piece))][static_cast<int>(utils::typeOfPiece(piece))] |=
        (1ULL << sq);
    board_[sq] = piece;

    occ_all_ |= (1ULL << sq);
}

inline void Board::removePiece(Piece piece, Square sq) {
    assert(board_[sq] == piece && piece != Piece::NONE);

    board_[sq] = Piece::NONE;

    pieces_bb_[int(color(piece))][int(utils::typeOfPiece(piece))] &= ~(1ULL << sq);

    occ_all_ &= ~(1ULL << sq);
}

inline void Board::makeMove(const Move &move) {
    const auto capture  = at(move.to()) != Piece::NONE && move.typeOf() != Move::CASTLING;
    const auto captured = at(move.to());
    const auto pt       = at<PieceType>(move.from());

    prev_states_.emplace_back(hash_key_, castling_rights_, enpassant_sq_, half_moves_, captured);

    half_moves_++;
    full_moves_++;

    if (enpassant_sq_ != NO_SQ) hash_key_ ^= zobrist::enpassant(utils::squareFile(enpassant_sq_));
    enpassant_sq_ = NO_SQ;

    if (capture) {
        half_moves_ = 0;

        hash_key_ ^= zobrist::piece(captured, move.to());
        removePiece(captured, move.to());

        const auto rank = utils::squareRank(move.to());

        if (utils::typeOfPiece(captured) == PieceType::ROOK &&
            ((rank == Rank::RANK_1 && side_to_move_ == Color::BLACK) ||
             (rank == Rank::RANK_8 && side_to_move_ == Color::WHITE))) {
            const auto king_sq = kingSq(~side_to_move_);
            const auto file = move.to() > king_sq ? CastleSide::KING_SIDE : CastleSide::QUEEN_SIDE;

            if (castling_rights_.getRookFile(~side_to_move_, file) ==
                utils::squareFile(move.to())) {
                const auto idx = castling_rights_.clearCastlingRight(~side_to_move_, file);
                hash_key_ ^= zobrist::castlingIndex(idx);
            }
        }
    }

    if (pt == PieceType::KING && castling_rights_.hasCastlingRight(side_to_move_)) {
        hash_key_ ^= zobrist::castling(castling_rights_.getHashIndex());

        castling_rights_.clearCastlingRight(side_to_move_);

        hash_key_ ^= zobrist::castling(castling_rights_.getHashIndex());

    } else if (pt == PieceType::ROOK && utils::ourBackRank(move.from(), side_to_move_)) {
        const auto king_sq = kingSq(side_to_move_);
        const auto file    = move.from() > king_sq ? CastleSide::KING_SIDE : CastleSide::QUEEN_SIDE;

        if (castling_rights_.getRookFile(side_to_move_, file) == utils::squareFile(move.from())) {
            const auto idx = castling_rights_.clearCastlingRight(side_to_move_, file);
            hash_key_ ^= zobrist::castlingIndex(idx);
        }
    } else if (pt == PieceType::PAWN) {
        half_moves_ = 0;

        const auto possible_ep = static_cast<Square>(move.to() ^ 8);
        if (std::abs(int(move.to()) - int(move.from())) == 16) {
            U64 ep_mask = attacks::pawn(side_to_move_, possible_ep);

            if (ep_mask & pieces(PieceType::PAWN, ~side_to_move_)) {
                enpassant_sq_ = possible_ep;

                hash_key_ ^= zobrist::enpassant(utils::squareFile(enpassant_sq_));
                assert(at(enpassant_sq_) == Piece::NONE);
            }
        }
    }

    if (move.typeOf() == Move::CASTLING) {
        assert(at<PieceType>(move.from()) == PieceType::KING);
        assert(at<PieceType>(move.to()) == PieceType::ROOK);

        bool king_side = move.to() > move.from();
        auto rookTo =
            utils::relativeSquare(side_to_move_, king_side ? Square::SQ_F1 : Square::SQ_D1);
        auto kingTo =
            utils::relativeSquare(side_to_move_, king_side ? Square::SQ_G1 : Square::SQ_C1);

        const auto king = at(move.from());
        const auto rook = at(move.to());

        removePiece(king, move.from());
        removePiece(rook, move.to());

        assert(king == utils::makePiece(side_to_move_, PieceType::KING));
        assert(rook == utils::makePiece(side_to_move_, PieceType::ROOK));

        placePiece(king, kingTo);
        placePiece(rook, rookTo);

        hash_key_ ^= zobrist::piece(king, move.from()) ^ zobrist::piece(king, kingTo);
        hash_key_ ^= zobrist::piece(rook, move.to()) ^ zobrist::piece(rook, rookTo);
    } else if (move.typeOf() == Move::PROMOTION) {
        const auto piece_pawn = utils::makePiece(side_to_move_, PieceType::PAWN);
        const auto piece_prom = utils::makePiece(side_to_move_, move.promotionType());

        removePiece(piece_pawn, move.from());
        placePiece(piece_prom, move.to());

        hash_key_ ^=
            zobrist::piece(piece_pawn, move.from()) ^ zobrist::piece(piece_prom, move.to());
    } else {
        assert(at(move.from()) != Piece::NONE);
        assert(at(move.to()) == Piece::NONE);
        const auto piece = at(move.from());

        removePiece(piece, move.from());
        placePiece(piece, move.to());

        hash_key_ ^= zobrist::piece(piece, move.from()) ^ zobrist::piece(piece, move.to());
    }

    if (move.typeOf() == Move::ENPASSANT) {
        assert(at<PieceType>(move.to() ^ 8) == PieceType::PAWN);

        const auto piece = utils::makePiece(~side_to_move_, PieceType::PAWN);

        removePiece(piece, Square(int(move.to()) ^ 8));

        hash_key_ ^= zobrist::piece(piece, Square(int(move.to()) ^ 8));
    }

    hash_key_ ^= zobrist::sideToMove();

    side_to_move_ = ~side_to_move_;
}

inline void Board::unmakeMove(const Move &move) {
    const auto prev = prev_states_.back();
    prev_states_.pop_back();

    enpassant_sq_    = prev.enpassant;
    castling_rights_ = prev.castling;
    half_moves_      = prev.half_moves;

    full_moves_--;

    side_to_move_ = ~side_to_move_;

    if (move.typeOf() == Move::CASTLING) {
        const bool king_side = move.to() > move.from();

        const auto rook_from_sq = utils::fileRankSquare(king_side ? File::FILE_F : File::FILE_D,
                                                        utils::squareRank(move.from()));
        const auto king_to_sq = utils::fileRankSquare(king_side ? File::FILE_G : File::FILE_C,
                                                      utils::squareRank(move.from()));

        assert(at<PieceType>(rook_from_sq) == PieceType::ROOK);
        assert(at<PieceType>(king_to_sq) == PieceType::KING);

        const auto rook = at(rook_from_sq);
        const auto king = at(king_to_sq);

        removePiece(rook, rook_from_sq);
        removePiece(king, king_to_sq);
        assert(king == utils::makePiece(side_to_move_, PieceType::KING));
        assert(rook == utils::makePiece(side_to_move_, PieceType::ROOK));

        placePiece(king, move.from());
        placePiece(rook, move.to());

        hash_key_ = prev.hash;

        return;
    } else if (move.typeOf() == Move::PROMOTION) {
        const auto pawn  = utils::makePiece(side_to_move_, PieceType::PAWN);
        const auto piece = at(move.to());
        assert(utils::typeOfPiece(piece) == move.promotionType());
        assert(utils::typeOfPiece(piece) != PieceType::PAWN);
        assert(utils::typeOfPiece(piece) != PieceType::KING);
        assert(utils::typeOfPiece(piece) != PieceType::NONE);

        removePiece(piece, move.to());
        placePiece(pawn, move.from());

        if (prev.captured_piece != Piece::NONE) {
            assert(at(move.to()) == Piece::NONE);
            placePiece(prev.captured_piece, move.to());
        }

        hash_key_ = prev.hash;
        return;
    } else {
        assert(at(move.to()) != Piece::NONE);

        const auto piece = at(move.to());
        assert(at(move.from()) == Piece::NONE);

        removePiece(piece, move.to());
        placePiece(piece, move.from());
    }

    if (move.typeOf() == Move::ENPASSANT) {
        const auto pawn   = utils::makePiece(~side_to_move_, PieceType::PAWN);
        const auto pawnTo = static_cast<Square>(enpassant_sq_ ^ 8);

        assert(at(pawnTo) == Piece::NONE);
        placePiece(pawn, pawnTo);
    } else if (prev.captured_piece != Piece::NONE) {
        assert(at(move.to()) == Piece::NONE);
        placePiece(prev.captured_piece, move.to());
    }

    hash_key_ = prev.hash;
}

inline void Board::makeNullMove() {
    prev_states_.emplace_back(hash_key_, castling_rights_, enpassant_sq_, half_moves_, Piece::NONE);

    hash_key_ ^= zobrist::sideToMove();
    if (enpassant_sq_ != NO_SQ) hash_key_ ^= zobrist::enpassant(utils::squareFile(enpassant_sq_));
    enpassant_sq_ = NO_SQ;

    side_to_move_ = ~side_to_move_;

    full_moves_++;
}

inline void Board::unmakeNullMove() {
    const auto &prev = prev_states_.back();

    enpassant_sq_    = prev.enpassant;
    castling_rights_ = prev.castling;
    half_moves_      = prev.half_moves;
    hash_key_        = prev.hash;

    full_moves_--;

    side_to_move_ = ~side_to_move_;

    prev_states_.pop_back();
}

/****************************************************************************\
 * Move Generation                                                           *
\****************************************************************************/

namespace movegen {

// force initialization of squares between
static auto init_squares_between = []() constexpr {
    // initialize squares between table
    std::array<std::array<U64, MAX_SQ>, MAX_SQ> squares_between_bb{};
    U64 sqs = 0;

    for (int sq1 = 0; sq1 < MAX_SQ; ++sq1) {
        for (int sq2 = 0; sq2 < MAX_SQ; ++sq2) {
            sqs = (1ULL << sq1) | (1ULL << sq2);
            if (sq1 == sq2)
                squares_between_bb[sq1][sq2] = 0ull;
            else if (utils::squareFile(Square(sq1)) == utils::squareFile(Square(sq2)) ||
                     utils::squareRank(Square(sq1)) == utils::squareRank(Square(sq2)))
                squares_between_bb[sq1][sq2] =
                    attacks::rook(Square(sq1), sqs) & attacks::rook(Square(sq2), sqs);
            else if (utils::diagonalOf(Square(sq1)) == utils::diagonalOf(Square(sq2)) ||
                     utils::antiDiagonalOf(Square(sq1)) == utils::antiDiagonalOf(Square(sq2)))
                squares_between_bb[sq1][sq2] =
                    attacks::bishop(Square(sq1), sqs) & attacks::bishop(Square(sq2), sqs);
        }
    }

    return squares_between_bb;
};

static const std::array<std::array<U64, 64>, 64> SQUARES_BETWEEN_BB = init_squares_between();

/// @brief [Internal Usage] Generate the checkmask.
/// Returns a bitboard where the attacker path between the king and enemy piece is set.
/// @tparam c
/// @param board
/// @param sq
/// @param double_check
/// @return
template <Color c>
[[nodiscard]] Bitboard checkMask(const Board &board, Square sq, int &double_check) {
    Bitboard mask = 0;
    double_check  = 0;

    const auto opp_knight = board.pieces(PieceType::KNIGHT, ~c);
    const auto opp_bishop = board.pieces(PieceType::BISHOP, ~c);
    const auto opp_rook   = board.pieces(PieceType::ROOK, ~c);
    const auto opp_queen  = board.pieces(PieceType::QUEEN, ~c);

    const auto opp_pawns = board.pieces(PieceType::PAWN, ~c);

    // check for knight checks
    Bitboard knight_attacks = attacks::knight(sq) & opp_knight;
    double_check += bool(knight_attacks);

    mask |= knight_attacks;

    // check for pawn checks
    Bitboard pawn_attacks = attacks::pawn(board.sideToMove(), sq) & opp_pawns;
    mask |= pawn_attacks;
    double_check += bool(pawn_attacks);

    // check for bishop checks
    Bitboard bishop_attacks = attacks::bishop(sq, board.occ()) & (opp_bishop | opp_queen);

    if (bishop_attacks) {
        const auto index = builtin::lsb(bishop_attacks);

        mask |= SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
        double_check++;
    }

    Bitboard rook_attacks = attacks::rook(sq, board.occ()) & (opp_rook | opp_queen);
    if (rook_attacks) {
        if (builtin::popcount(rook_attacks) > 1) {
            double_check = 2;
            return mask;
        }

        const auto index = builtin::lsb(rook_attacks);

        mask |= SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
        double_check++;
    }

    if (!mask) {
        return DEFAULT_CHECKMASK;
    }

    return mask;
}

/// @brief [Internal Usage] Generate the pin mask for horizontal and vertical pins.
/// Returns a bitboard where the ray between the king and the pinner is set.
/// @tparam c
/// @param board
/// @param sq
/// @param occ_enemy
/// @param occ_us
/// @return
template <Color c>
[[nodiscard]] Bitboard pinMaskRooks(const Board &board, Square sq, Bitboard occ_enemy,
                                    Bitboard occ_us) {
    Bitboard pin_hv = 0;

    const auto opp_rook  = board.pieces(PieceType::ROOK, ~c);
    const auto opp_queen = board.pieces(PieceType::QUEEN, ~c);

    Bitboard rook_attacks = attacks::rook(sq, occ_enemy) & (opp_rook | opp_queen);

    while (rook_attacks) {
        const auto index = builtin::poplsb(rook_attacks);

        const Bitboard possible_pin = SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
        if (builtin::popcount(possible_pin & occ_us) == 1) pin_hv |= possible_pin;
    }

    return pin_hv;
}

/// @brief [Internal Usage] Generate the pin mask for diagonal pins.
/// Returns a bitboard where the ray between the king and the pinner is set.
/// @tparam c
/// @param board
/// @param sq
/// @param occ_enemy
/// @param occ_us
/// @return
template <Color c>
[[nodiscard]] Bitboard pinMaskBishops(const Board &board, Square sq, Bitboard occ_enemy,
                                      Bitboard occ_us) {
    Bitboard pin_diag = 0;

    const auto opp_bishop = board.pieces(PieceType::BISHOP, ~c);
    const auto opp_queen  = board.pieces(PieceType::QUEEN, ~c);

    Bitboard bishop_attacks = attacks::bishop(sq, occ_enemy) & (opp_bishop | opp_queen);

    while (bishop_attacks) {
        const auto index = builtin::poplsb(bishop_attacks);

        const Bitboard possible_pin = SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
        if (builtin::popcount(possible_pin & occ_us) == 1) pin_diag |= possible_pin;
    }

    return pin_diag;
}

/// @brief [Internal Usage] Returns the squares that are attacked by the enemy
/// @tparam c
/// @param board
/// @param enemy_empty
/// @return
template <Color c>
[[nodiscard]] Bitboard seenSquares(const Board &board, Bitboard enemy_empty) {
    auto king_sq = board.kingSq(~c);

    auto queens  = board.pieces(PieceType::QUEEN, c);
    auto pawns   = board.pieces(PieceType::PAWN, c);
    auto knights = board.pieces(PieceType::KNIGHT, c);
    auto bishops = board.pieces(PieceType::BISHOP, c) | queens;
    auto rooks   = board.pieces(PieceType::ROOK, c) | queens;

    auto occ = board.occ();

    Bitboard map_king_atk = attacks::king(king_sq) & enemy_empty;

    if (map_king_atk == 0ull && !board.chess960()) {
        return 0ull;
    }

    occ &= ~(1ULL << king_sq);

    Bitboard seen = attacks::pawnLeftAttacks<c>(pawns) | attacks::pawnRightAttacks<c>(pawns);

    while (knights) {
        const auto index = builtin::poplsb(knights);
        seen |= attacks::knight(index);
    }

    while (bishops) {
        const auto index = builtin::poplsb(bishops);
        seen |= attacks::bishop(index, occ);
    }

    while (rooks) {
        const auto index = builtin::poplsb(rooks);
        seen |= attacks::rook(index, occ);
    }

    const Square index = board.kingSq(c);
    seen |= attacks::king(index);

    return seen;
}

/// @brief [Internal Usage] Generate pawn moves.
/// @tparam c
/// @tparam mt
/// @param board
/// @param moves
/// @param pin_d
/// @param pin_hv
/// @param checkmask
/// @param occ_enemy
template <Color c, MoveGenType mt>
void generatePawnMoves(const Board &board, Movelist &moves, Bitboard pin_d, Bitboard pin_hv,
                       Bitboard checkmask, Bitboard occ_enemy) {
    const auto pawns = board.pieces(PieceType::PAWN, c);

    constexpr Direction UP   = c == Color::WHITE ? Direction::NORTH : Direction::SOUTH;
    constexpr Direction DOWN = c == Color::WHITE ? Direction::SOUTH : Direction::NORTH;
    constexpr Direction DOWN_LEFT =
        c == Color::WHITE ? Direction::SOUTH_WEST : Direction::NORTH_EAST;
    constexpr Direction DOWN_RIGHT =
        c == Color::WHITE ? Direction::SOUTH_EAST : Direction::NORTH_WEST;

    constexpr Bitboard RANK_B_PROMO     = c == Color::WHITE
                                              ? attacks::MASK_RANK[static_cast<int>(Rank::RANK_7)]
                                              : attacks::MASK_RANK[static_cast<int>(Rank::RANK_2)];
    constexpr Bitboard RANK_PROMO       = c == Color::WHITE
                                              ? attacks::MASK_RANK[static_cast<int>(Rank::RANK_8)]
                                              : attacks::MASK_RANK[static_cast<int>(Rank::RANK_1)];
    constexpr Bitboard DOUBLE_PUSH_RANK = c == Color::WHITE
                                              ? attacks::MASK_RANK[static_cast<int>(Rank::RANK_3)]
                                              : attacks::MASK_RANK[static_cast<int>(Rank::RANK_6)];

    // These pawns can maybe take Left or Right
    const Bitboard pawns_lr = pawns & ~pin_hv;

    const Bitboard unpinnedpawns_lr = pawns_lr & ~pin_d;
    const Bitboard pinnedpawns_lr   = pawns_lr & pin_d;

    Bitboard l_pawns = (attacks::pawnLeftAttacks<c>(unpinnedpawns_lr)) |
                       (attacks::pawnLeftAttacks<c>(pinnedpawns_lr) & pin_d);

    Bitboard r_pawns = (attacks::pawnRightAttacks<c>(unpinnedpawns_lr)) |
                       (attacks::pawnRightAttacks<c>(pinnedpawns_lr) & pin_d);

    // Prune moves that don't capture a piece and are not on the checkmask.
    l_pawns &= occ_enemy & checkmask;
    r_pawns &= occ_enemy & checkmask;

    // These pawns can walk Forward
    const Bitboard pawns_hv = pawns & ~pin_d;

    const Bitboard pawns_pinned_hv   = pawns_hv & pin_hv;
    const Bitboard pawns_unpinned_hv = pawns_hv & ~pin_hv;

    // Prune moves that are blocked by a piece
    const Bitboard single_push_unpinned = attacks::shift<UP>(pawns_unpinned_hv) & ~board.occ();
    const Bitboard single_push_pinned = attacks::shift<UP>(pawns_pinned_hv) & pin_hv & ~board.occ();

    // Prune moves that are not on the checkmask.
    Bitboard single_push = (single_push_unpinned | single_push_pinned) & checkmask;

    Bitboard double_push =
        ((attacks::shift<UP>(single_push_unpinned & DOUBLE_PUSH_RANK) & ~board.occ()) |
         (attacks::shift<UP>(single_push_pinned & DOUBLE_PUSH_RANK) & ~board.occ())) &
        checkmask;

    if (pawns & RANK_B_PROMO) {
        Bitboard promo_left  = l_pawns & RANK_PROMO;
        Bitboard promo_right = r_pawns & RANK_PROMO;
        Bitboard promo_push  = single_push & RANK_PROMO;

        // Skip capturing promotions if we are only generating quiet moves.
        // Generates at ALL and CAPTURE
        while (mt != MoveGenType::QUIET && promo_left) {
            const auto index = builtin::poplsb(promo_left);
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_RIGHT, index, PieceType::QUEEN));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_RIGHT, index, PieceType::ROOK));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_RIGHT, index, PieceType::BISHOP));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_RIGHT, index, PieceType::KNIGHT));
        }

        // Skip capturing promotions if we are only generating quiet moves.
        // Generates at ALL and CAPTURE
        while (mt != MoveGenType::QUIET && promo_right) {
            const auto index = builtin::poplsb(promo_right);
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_LEFT, index, PieceType::QUEEN));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_LEFT, index, PieceType::ROOK));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_LEFT, index, PieceType::BISHOP));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN_LEFT, index, PieceType::KNIGHT));
        }

        // Skip quiet promotions if we are only generating captures.
        // Generates at ALL and QUIET
        while (mt != MoveGenType::CAPTURE && promo_push) {
            const auto index = builtin::poplsb(promo_push);
            moves.add(Move::make<Move::PROMOTION>(index + DOWN, index, PieceType::QUEEN));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN, index, PieceType::ROOK));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN, index, PieceType::BISHOP));
            moves.add(Move::make<Move::PROMOTION>(index + DOWN, index, PieceType::KNIGHT));
        }
    }

    single_push &= ~RANK_PROMO;
    l_pawns &= ~RANK_PROMO;
    r_pawns &= ~RANK_PROMO;

    while (mt != MoveGenType::QUIET && l_pawns) {
        const auto index = builtin::poplsb(l_pawns);
        moves.add(Move::make<Move::NORMAL>(index + DOWN_RIGHT, index));
    }

    while (mt != MoveGenType::QUIET && r_pawns) {
        const auto index = builtin::poplsb(r_pawns);
        moves.add(Move::make<Move::NORMAL>(index + DOWN_LEFT, index));
    }

    while (mt != MoveGenType::CAPTURE && single_push) {
        const auto index = builtin::poplsb(single_push);
        moves.add(Move::make<Move::NORMAL>(index + DOWN, index));
    }

    while (mt != MoveGenType::CAPTURE && double_push) {
        const auto index = builtin::poplsb(double_push);
        moves.add(Move::make<Move::NORMAL>(index + DOWN + DOWN, index));
    }

    const Square ep = board.enpassantSq();
    if (mt != MoveGenType::QUIET && ep != NO_SQ) {
        const Square epPawn = ep + DOWN;

        const Bitboard ep_mask = (1ull << epPawn) | (1ull << ep);

        /*
         In case the en passant square and the enemy pawn
         that just moved are not on the checkmask
         en passant is not available.
        */
        if ((checkmask & ep_mask) == 0) return;

        const Square kSQ = board.kingSq(c);
        const Bitboard kingMask =
            (1ull << kSQ) & attacks::MASK_RANK[static_cast<int>(utils::squareRank(epPawn))];
        const Bitboard enemyQueenRook =
            board.pieces(PieceType::ROOK, ~c) | board.pieces(PieceType::QUEEN, ~c);

        const bool isPossiblePin = kingMask && enemyQueenRook;
        Bitboard epBB            = attacks::pawn(~c, ep) & pawns_lr;

        // For one en passant square two pawns could potentially take there.

        while (epBB) {
            const Square from = builtin::poplsb(epBB);
            const Square to   = ep;

            /*
             If the pawn is pinned but the en passant square is not on the
             pin mask then the move is illegal.
            */
            if ((1ULL << from) & pin_d && !(pin_d & (1ull << ep))) continue;

            const Bitboard connectingPawns = (1ull << epPawn) | (1ull << from);

            /*
             7k/4p3/8/2KP3r/8/8/8/8 b - - 0 1
             If e7e5 there will be a potential ep square for us on e6.
             However, we cannot take en passant because that would put our king
             in check. For this scenario we check if there's an enemy rook/queen
             that would give check if the two pawns were removed.
             If that's the case then the move is illegal and we can break immediately.
            */
            if (isPossiblePin &&
                (attacks::rook(kSQ, board.occ() & ~connectingPawns) & enemyQueenRook) != 0)
                break;

            moves.add(Move::make<Move::ENPASSANT>(from, to));
        }
    }
}

/// @brief [Internal Usage] Generate knight moves.
/// @param sq
/// @param movable
/// @return
[[nodiscard]] inline Bitboard generateKnightMoves(Square sq, Bitboard movable) {
    return attacks::knight(sq) & movable;
}

/// @brief [Internal Usage] Generate bishop moves.
/// @param sq
/// @param movable
/// @param pin_d
/// @param occ_all
/// @return
[[nodiscard]] inline Bitboard generateBishopMoves(Square sq, Bitboard movable, Bitboard pin_d,
                                                  Bitboard occ_all) {
    // The Bishop is pinned diagonally thus can only move diagonally.
    if (pin_d & (1ULL << sq)) return attacks::bishop(sq, occ_all) & movable & pin_d;
    return attacks::bishop(sq, occ_all) & movable;
}

/// @brief [Internal Usage] Generate rook moves.
/// @param sq
/// @param movable
/// @param pin_hv
/// @param occ_all
/// @return
[[nodiscard]] inline Bitboard generateRookMoves(Square sq, Bitboard movable, Bitboard pin_hv,
                                                Bitboard occ_all) {
    // The Rook is pinned horizontally thus can only move horizontally.
    if (pin_hv & (1ULL << sq)) return attacks::rook(sq, occ_all) & movable & pin_hv;
    return attacks::rook(sq, occ_all) & movable;
}

/// @brief [Internal Usage] Generate queen moves.
/// @param sq
/// @param movable
/// @param pin_d
/// @param pin_hv
/// @param occ_all
/// @return
[[nodiscard]] inline Bitboard generateQueenMoves(Square sq, Bitboard movable, Bitboard pin_d,
                                                 Bitboard pin_hv, Bitboard occ_all) {
    Bitboard moves = 0ULL;

    if (pin_d & (1ULL << sq))
        moves |= attacks::bishop(sq, occ_all) & movable & pin_d;
    else if (pin_hv & (1ULL << sq))
        moves |= attacks::rook(sq, occ_all) & movable & pin_hv;
    else {
        moves |= attacks::rook(sq, occ_all) & movable;
        moves |= attacks::bishop(sq, occ_all) & movable;
    }

    return moves;
}

/// @brief [Internal Usage] Generate king moves.
/// @param sq
/// @param _seen
/// @param movable_square
/// @return
[[nodiscard]] inline Bitboard generateKingMoves(Square sq, Bitboard _seen,
                                                Bitboard movable_square) {
    return attacks::king(sq) & movable_square & ~_seen;
}

/// @brief [Internal Usage] Generate castling moves.
/// @tparam c
/// @tparam mt
/// @param board
/// @param sq
/// @param seen
/// @param pinHV
/// @return
template <Color c, MoveGenType mt>
[[nodiscard]] inline Bitboard generateCastleMoves(const Board &board, Square sq, Bitboard seen,
                                                  Bitboard pinHV) {
    if constexpr (mt == MoveGenType::CAPTURE) return 0ull;
    const auto rights = board.castlingRights();

    Bitboard moves = 0ull;

    for (const auto side : {CastleSide::KING_SIDE, CastleSide::QUEEN_SIDE}) {
        if (!rights.hasCastlingRight(c, side)) continue;

        const auto end_king_sq =
            utils::relativeSquare(c, side == CastleSide::KING_SIDE ? Square::SQ_G1 : Square::SQ_C1);
        const auto end_rook_sq =
            utils::relativeSquare(c, side == CastleSide::KING_SIDE ? Square::SQ_F1 : Square::SQ_D1);

        const auto from_rook_sq =
            utils::fileRankSquare(rights.getRookFile(c, side), utils::squareRank(sq));

        const Bitboard not_occ_path       = SQUARES_BETWEEN_BB[sq][from_rook_sq];
        const Bitboard not_attacked_path  = SQUARES_BETWEEN_BB[sq][end_king_sq];
        const Bitboard empty_not_attacked = ~seen & ~(board.occ() & ~(1ull << from_rook_sq));
        const Bitboard withoutRook        = board.occ() & ~(1ull << from_rook_sq);
        const Bitboard withoutKing        = board.occ() & ~(1ull << sq);

        if ((not_attacked_path & empty_not_attacked) == not_attacked_path &&
            ((not_occ_path & ~board.occ()) == not_occ_path) &&
            !((1ull << from_rook_sq) & pinHV &
              attacks::MASK_RANK[static_cast<int>(utils::squareRank(sq))]) &&
            !((1ull << end_rook_sq) & (withoutRook & withoutKing)) &&
            !((1ull << end_king_sq) & (seen | (withoutRook & ~(1ull << sq))))) {
            moves |= (1ull << from_rook_sq);
        }
    }

    return moves;
}

/// @brief [Internal Usage] all legal moves for a position
/// @tparam c
/// @tparam mt
/// @param movelist
/// @param board
template <Color c, MoveGenType mt>
void legalmoves(Movelist &movelist, const Board &board) {
    /*
     The size of the movelist might not
     be 0! This is done on purpose since it enables
     you to append new move types to any movelist.
    */
    auto king_sq = board.kingSq(c);

    int _doubleCheck = 0;

    Bitboard _occ_us        = board.us(c);
    Bitboard _occ_enemy     = board.us(~c);
    Bitboard _occ_all       = _occ_us | _occ_enemy;
    Bitboard _enemy_emptyBB = ~_occ_us;

    Bitboard _seen      = seenSquares<~c>(board, _enemy_emptyBB);
    Bitboard _checkMask = checkMask<c>(board, king_sq, _doubleCheck);
    Bitboard _pinHV     = pinMaskRooks<c>(board, king_sq, _occ_enemy, _occ_us);
    Bitboard _pinD      = pinMaskBishops<c>(board, king_sq, _occ_enemy, _occ_us);

    assert(_doubleCheck <= 2);

    // Moves have to be on the checkmask
    Bitboard movable_square;

    // Slider, Knights and King moves can only go to enemy or empty squares.
    if (mt == MoveGenType::ALL)
        movable_square = _enemy_emptyBB;
    else if (mt == MoveGenType::CAPTURE)
        movable_square = _occ_enemy;
    else  // QUIET moves
        movable_square = ~_occ_all;

    Bitboard moves = generateKingMoves(king_sq, _seen, movable_square);

    movable_square &= _checkMask;

    while (moves) {
        Square to = builtin::poplsb(moves);
        movelist.add(Move::make<Move::NORMAL>(king_sq, to));
    }

    if (utils::squareRank(king_sq) == (c == Color::WHITE ? Rank::RANK_1 : Rank::RANK_8) &&
        (board.castlingRights().hasCastlingRight(c) && _checkMask == DEFAULT_CHECKMASK)) {
        moves = generateCastleMoves<c, mt>(board, king_sq, _seen, _pinHV);

        while (moves) {
            Square to = builtin::poplsb(moves);
            movelist.add(Move::make<Move::CASTLING>(king_sq, to));
        }
    }

    // Early return for double check as described earlier
    if (_doubleCheck == 2) return;

    // Prune knights that are pinned since these cannot move.
    Bitboard knights_mask = board.pieces(PieceType::KNIGHT, c) & ~(_pinD | _pinHV);

    // Prune horizontally pinned bishops
    Bitboard bishops_mask = board.pieces(PieceType::BISHOP, c) & ~_pinHV;

    //  Prune diagonally pinned rooks
    Bitboard rooks_mask = board.pieces(PieceType::ROOK, c) & ~_pinD;

    // Prune double pinned queens
    Bitboard queens_mask = board.pieces(PieceType::QUEEN, c) & ~(_pinD & _pinHV);

    // Add the moves to the movelist.
    generatePawnMoves<c, mt>(board, movelist, _pinD, _pinHV, _checkMask, _occ_enemy);

    while (knights_mask) {
        const Square from = builtin::poplsb(knights_mask);
        moves             = generateKnightMoves(from, movable_square);
        while (moves) {
            const Square to = builtin::poplsb(moves);
            movelist.add(Move::make<Move::NORMAL>(from, to));
        }
    }

    while (bishops_mask) {
        const Square from = builtin::poplsb(bishops_mask);
        moves             = generateBishopMoves(from, movable_square, _pinD, _occ_all);
        while (moves) {
            const Square to = builtin::poplsb(moves);
            movelist.add(Move::make<Move::NORMAL>(from, to));
        }
    }

    while (rooks_mask) {
        const Square from = builtin::poplsb(rooks_mask);
        moves             = generateRookMoves(from, movable_square, _pinHV, _occ_all);
        while (moves) {
            const Square to = builtin::poplsb(moves);
            movelist.add(Move::make<Move::NORMAL>(from, to));
        }
    }

    while (queens_mask) {
        const Square from = builtin::poplsb(queens_mask);
        moves             = generateQueenMoves(from, movable_square, _pinD, _pinHV, _occ_all);
        while (moves) {
            const Square to = builtin::poplsb(moves);
            movelist.add(Move::make<Move::NORMAL>(from, to));
        }
    }
}

template <MoveGenType mt>
inline void legalmoves(Movelist &movelist, const Board &board) {
    movelist.clear();

    if (board.sideToMove() == Color::WHITE)
        legalmoves<Color::WHITE, mt>(movelist, board);
    else
        legalmoves<Color::BLACK, mt>(movelist, board);
}

}  // namespace movegen

namespace attacks {

/// @brief Shifts a bitboard in a given direction
/// @tparam direction
/// @param b
/// @return
template <Direction direction>
[[nodiscard]] constexpr Bitboard shift(const Bitboard b) {
    switch (direction) {
        case Direction::NORTH:
            return b << 8;
        case Direction::SOUTH:
            return b >> 8;
        case Direction::NORTH_WEST:
            return (b & ~MASK_FILE[0]) << 7;
        case Direction::WEST:
            return (b & ~MASK_FILE[0]) >> 1;
        case Direction::SOUTH_WEST:
            return (b & ~MASK_FILE[0]) >> 9;
        case Direction::NORTH_EAST:
            return (b & ~MASK_FILE[7]) << 9;
        case Direction::EAST:
            return (b & ~MASK_FILE[7]) << 1;
        case Direction::SOUTH_EAST:
            return (b & ~MASK_FILE[7]) >> 7;
    }
}

// clang-format off
// pre-calculated lookup table for pawn attacks
static constexpr Bitboard PawnAttacks[2][MAX_SQ] = {
    // white pawn attacks
    { 0x200, 0x500, 0xa00, 0x1400,
      0x2800, 0x5000, 0xa000, 0x4000,
      0x20000, 0x50000, 0xa0000, 0x140000,
      0x280000, 0x500000, 0xa00000, 0x400000,
      0x2000000, 0x5000000, 0xa000000, 0x14000000,
      0x28000000, 0x50000000, 0xa0000000, 0x40000000,
      0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
      0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
      0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
      0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
      0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
      0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000,
      0x200000000000000, 0x500000000000000, 0xa00000000000000, 0x1400000000000000,
      0x2800000000000000, 0x5000000000000000, 0xa000000000000000, 0x4000000000000000,
      0x0, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0 },

      // black pawn attacks
      { 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0,
        0x2, 0x5, 0xa, 0x14,
        0x28, 0x50, 0xa0, 0x40,
        0x200, 0x500, 0xa00, 0x1400,
        0x2800, 0x5000, 0xa000, 0x4000,
        0x20000, 0x50000, 0xa0000, 0x140000,
        0x280000, 0x500000, 0xa00000, 0x400000,
        0x2000000, 0x5000000, 0xa000000, 0x14000000,
        0x28000000, 0x50000000, 0xa0000000, 0x40000000,
        0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
        0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
        0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
        0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
        0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
        0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000
      }
};

// clang-format on

// pre-calculated lookup table for knight attacks
static constexpr Bitboard KnightAttacks[MAX_SQ] = {
    0x0000000000020400, 0x0000000000050800, 0x00000000000A1100, 0x0000000000142200,
    0x0000000000284400, 0x0000000000508800, 0x0000000000A01000, 0x0000000000402000,
    0x0000000002040004, 0x0000000005080008, 0x000000000A110011, 0x0000000014220022,
    0x0000000028440044, 0x0000000050880088, 0x00000000A0100010, 0x0000000040200020,
    0x0000000204000402, 0x0000000508000805, 0x0000000A1100110A, 0x0000001422002214,
    0x0000002844004428, 0x0000005088008850, 0x000000A0100010A0, 0x0000004020002040,
    0x0000020400040200, 0x0000050800080500, 0x00000A1100110A00, 0x0000142200221400,
    0x0000284400442800, 0x0000508800885000, 0x0000A0100010A000, 0x0000402000204000,
    0x0002040004020000, 0x0005080008050000, 0x000A1100110A0000, 0x0014220022140000,
    0x0028440044280000, 0x0050880088500000, 0x00A0100010A00000, 0x0040200020400000,
    0x0204000402000000, 0x0508000805000000, 0x0A1100110A000000, 0x1422002214000000,
    0x2844004428000000, 0x5088008850000000, 0xA0100010A0000000, 0x4020002040000000,
    0x0400040200000000, 0x0800080500000000, 0x1100110A00000000, 0x2200221400000000,
    0x4400442800000000, 0x8800885000000000, 0x100010A000000000, 0x2000204000000000,
    0x0004020000000000, 0x0008050000000000, 0x00110A0000000000, 0x0022140000000000,
    0x0044280000000000, 0x0088500000000000, 0x0010A00000000000, 0x0020400000000000};

// pre-calculated lookup table for king attacks
static constexpr Bitboard KingAttacks[MAX_SQ] = {
    0x0000000000000302, 0x0000000000000705, 0x0000000000000E0A, 0x0000000000001C14,
    0x0000000000003828, 0x0000000000007050, 0x000000000000E0A0, 0x000000000000C040,
    0x0000000000030203, 0x0000000000070507, 0x00000000000E0A0E, 0x00000000001C141C,
    0x0000000000382838, 0x0000000000705070, 0x0000000000E0A0E0, 0x0000000000C040C0,
    0x0000000003020300, 0x0000000007050700, 0x000000000E0A0E00, 0x000000001C141C00,
    0x0000000038283800, 0x0000000070507000, 0x00000000E0A0E000, 0x00000000C040C000,
    0x0000000302030000, 0x0000000705070000, 0x0000000E0A0E0000, 0x0000001C141C0000,
    0x0000003828380000, 0x0000007050700000, 0x000000E0A0E00000, 0x000000C040C00000,
    0x0000030203000000, 0x0000070507000000, 0x00000E0A0E000000, 0x00001C141C000000,
    0x0000382838000000, 0x0000705070000000, 0x0000E0A0E0000000, 0x0000C040C0000000,
    0x0003020300000000, 0x0007050700000000, 0x000E0A0E00000000, 0x001C141C00000000,
    0x0038283800000000, 0x0070507000000000, 0x00E0A0E000000000, 0x00C040C000000000,
    0x0302030000000000, 0x0705070000000000, 0x0E0A0E0000000000, 0x1C141C0000000000,
    0x3828380000000000, 0x7050700000000000, 0xE0A0E00000000000, 0xC040C00000000000,
    0x0203000000000000, 0x0507000000000000, 0x0A0E000000000000, 0x141C000000000000,
    0x2838000000000000, 0x5070000000000000, 0xA0E0000000000000, 0x40C0000000000000};

/// @brief [Internal Usage] Generate the left side pawn attacks.
/// @tparam c
/// @param pawns
/// @return
template <Color c>
[[nodiscard]] Bitboard pawnLeftAttacks(const Bitboard pawns) {
    return c == Color::WHITE ? (pawns << 7) & ~MASK_FILE[static_cast<int>(File::FILE_H)]
                             : (pawns >> 7) & ~MASK_FILE[static_cast<int>(File::FILE_A)];
}

/// @brief [Internal Usage] Generate the right side pawn attacks.
/// @tparam c
/// @param pawns
/// @return
template <Color c>
[[nodiscard]] Bitboard pawnRightAttacks(const Bitboard pawns) {
    return c == Color::WHITE ? (pawns << 9) & ~MASK_FILE[static_cast<int>(File::FILE_A)]
                             : (pawns >> 9) & ~MASK_FILE[static_cast<int>(File::FILE_H)];
}

/// @brief Returns the pawn attacks for a given color and square
/// @param c
/// @param sq
/// @return
[[nodiscard]] inline Bitboard pawn(Color c, Square sq) { return PawnAttacks[int(c)][sq]; }

/// @brief Returns the knight attacks for a given square
/// @param sq
/// @return
[[nodiscard]] inline Bitboard knight(Square sq) { return KnightAttacks[sq]; }

/// @brief Returns the bishop attacks for a given square
/// @param sq
/// @param occupied
/// @return
[[nodiscard]] inline Bitboard bishop(Square sq, Bitboard occupied) {
    return BishopTable[sq].attacks[BishopTable[sq](occupied)];
}

/// @brief Returns the rook attacks for a given square
/// @param sq
/// @param occupied
/// @return
[[nodiscard]] inline Bitboard rook(Square sq, Bitboard occupied) {
    return RookTable[sq].attacks[RookTable[sq](occupied)];
}

/// @brief Returns the queen attacks for a given square
/// @param sq
/// @param occupied
/// @return
[[nodiscard]] inline Bitboard queen(Square sq, Bitboard occupied) {
    return bishop(sq, occupied) | rook(sq, occupied);
}

/// @brief Returns the king attacks for a given square
/// @param sq
/// @return
[[nodiscard]] inline Bitboard king(Square sq) { return KingAttacks[sq]; }

/// @brief Returns a bitboard with the origin squares of the attacking pieces set
/// @param board
/// @param color Attacker Color
/// @param square Attacked Square
/// @param occupied
/// @return
[[nodiscard]] inline Bitboard attackers(const Board &board, Color color, Square square,
                                        Bitboard occupied) {
    const auto queens = board.pieces(PieceType::QUEEN, color);

    // using the fact that if we can attack PieceType from square, they can attack us back
    auto atks = (pawn(~color, square) & board.pieces(PieceType::PAWN, color));
    atks |= (knight(square) & board.pieces(PieceType::KNIGHT, color));
    atks |= (bishop(square, occupied) & (board.pieces(PieceType::BISHOP, color) | queens));
    atks |= (rook(square, occupied) & (board.pieces(PieceType::ROOK, color) | queens));
    atks |= (king(square) & board.pieces(PieceType::KING, color));

    return atks & occupied;
}

}  // namespace attacks

/****************************************************************************\
 * uci utility functions                                                     *
\****************************************************************************/

namespace uci {
/// @brief Converts an internal move to a UCI string
/// @param move
/// @param chess960
/// @return
[[nodiscard]] inline std::string moveToUci(const Move &move, bool chess960 = false) {
    std::stringstream ss;

    // Get the from and to squares
    Square from_sq = move.from();
    Square to_sq   = move.to();

    // If the move is not a chess960 castling move and is a king moving more than one square,
    // update the to square to be the correct square for a regular castling move
    if (!chess960 && move.typeOf() == Move::CASTLING) {
        to_sq = utils::fileRankSquare(to_sq > from_sq ? File::FILE_G : File::FILE_C,
                                      utils::squareRank(from_sq));
    }

    // Add the from and to squares to the string stream
    ss << squareToString[from_sq];
    ss << squareToString[to_sq];

    // If the move is a promotion, add the promoted piece to the string stream
    if (move.typeOf() == Move::PROMOTION) {
        ss << PieceTypeToChar[move.promotionType()];
    }

    return ss.str();
}

/// @brief Converts a UCI string to an internal move.
/// @param board
/// @param uci
/// @return
[[nodiscard]] inline Move uciToMove(const Board &board, const std::string &uci) {
    Square source   = utils::extractSquare(uci.substr(0, 2));
    Square target   = utils::extractSquare(uci.substr(2, 2));
    PieceType piece = utils::typeOfPiece(board.at(source));

    // convert to king captures rook
    // in chess960 the move should be sent as king captures rook already!
    if (piece == PieceType::KING && utils::squareDistance(target, source) == 2) {
        target = utils::fileRankSquare(target > source ? File::FILE_H : File::FILE_A,
                                       utils::squareRank(source));
        return Move::make<Move::CASTLING>(source, target);
    }

    // en passant
    if (piece == PieceType::PAWN && target == board.enpassantSq()) {
        return Move::make<Move::ENPASSANT>(source, target);
    }

    // promotion
    if (piece == PieceType::PAWN && uci.length() == 5 &&
        utils::squareRank(target) ==
            (board.sideToMove() == Color::WHITE ? Rank::RANK_8 : Rank::RANK_1)) {
        return Move::make<Move::PROMOTION>(source, target, charToPieceType[uci.at(4)]);
    }

    switch (uci.length()) {
        case 4:
            return Move::make<Move::NORMAL>(source, target);
        default:
            std::cout << "Warning; uci move cannot be converted to move!" << std::endl;
            return Move::make<Move::NORMAL>(source, target);
    }
}

/// @brief Converts a move to a SAN string
/// @param board
/// @param move
/// @return
[[nodiscard]] inline std::string moveToSan(Board board, const Move &move) {
    static const std::string repPieceType[] = {"", "N", "B", "R", "Q", "K"};
    static const std::string repFile[]      = {"a", "b", "c", "d", "e", "f", "g", "h"};

    if (move.typeOf() == Move::CASTLING) {
        return move.to() > move.from() ? "O-O" : "O-O-O";
    }

    const PieceType pt = utils::typeOfPiece(board.at(move.from()));

    assert(pt != PieceType::NONE);

    std::string san;

    if (pt != PieceType::PAWN) {
        san += repPieceType[int(pt)];
    }

    Movelist moves;
    movegen::legalmoves(moves, board);

    for (const auto &m : moves) {
        if (pt != PieceType::PAWN && m != move && board.at(m.from()) == board.at(move.from()) &&
            m.to() == move.to()) {
            if (utils::squareFile(m.from()) == utils::squareFile(move.from())) {
                san += std::to_string(int(utils::squareRank(move.from())) + 1);
                break;
            } else {
                san += repFile[int(utils::squareFile(move.from()))];
                break;
            }
        }
    }

    if (board.at(move.to()) != Piece::NONE || move.typeOf() == Move::ENPASSANT) {
        if (pt == PieceType::PAWN) {
            san += repFile[int(utils::squareFile(move.from()))];
        }

        san += "x";
    }

    san += repFile[int(utils::squareFile(move.to()))];
    san += std::to_string(int(utils::squareRank(move.to())) + 1);

    if (move.typeOf() == Move::PROMOTION) {
        san += "=";
        san += repPieceType[int(move.promotionType())];
    }

    board.makeMove(move);

    if (board.inCheck()) {
        if (board.isGameOver().second == GameResult::LOSE) {
            san += "#";
        } else {
            san += "+";
        }
    }

    return san;
}

/// @brief Converts a move to a LAN string
/// @param board
/// @param move
/// @return
[[nodiscard]] inline std::string moveToLan(Board board, const Move &move) {
    static const std::string repPieceType[] = {"", "N", "B", "R", "Q", "K"};
    static const std::string repFile[]      = {"a", "b", "c", "d", "e", "f", "g", "h"};

    if (move.typeOf() == Move::CASTLING) {
        return move.to() > move.from() ? "O-O" : "O-O-O";
    }

    const PieceType pt = utils::typeOfPiece(board.at(move.from()));

    assert(pt != PieceType::NONE);

    std::string lan;

    if (pt != PieceType::PAWN) {
        lan += repPieceType[int(pt)];
    }

    lan += repFile[int(utils::squareFile(move.from()))];
    lan += std::to_string(int(utils::squareRank(move.from())) + 1);

    if (board.at(move.to()) != Piece::NONE || move.typeOf() == Move::ENPASSANT) {
        if (pt == PieceType::PAWN) {
            lan += repFile[int(utils::squareFile(move.from()))];
        }

        lan += "x";
    }

    lan += repFile[int(utils::squareFile(move.to()))];
    lan += std::to_string(int(utils::squareRank(move.to())) + 1);

    if (move.typeOf() == Move::PROMOTION) {
        lan += "=";
        lan += repPieceType[int(move.promotionType())];
    }

    board.makeMove(move);

    if (board.inCheck()) {
        if (board.isGameOver().second == GameResult::LOSE) {
            lan += "#";
        } else {
            lan += "+";
        }
    }

    return lan;
}

/// @brief Converts a SAN string to a move
/// @param board
/// @param san
/// @return
[[nodiscard]] inline Move parseSan(const Board &board, std::string san) {
    Movelist moves;
    movegen::legalmoves(moves, board);

    const auto original = san;

    if (san == "0-0" || san == "0-0+" || san == "0-0#" || san == "O-O" || san == "O-O+" ||
        san == "O-O#") {
        for (auto move : moves) {
            if (move.typeOf() == Move::CASTLING && move.to() > move.from()) {
                return move;
            }
        }

        throw std::runtime_error("Illegal San, Step 1: " + san);

    } else if (san == "0-0-0" || san == "0-0-0+" || san == "0-0-0#" || san == "O-O-O" ||
               san == "O-O-O+" || san == "O-O-O#") {
        for (auto move : moves) {
            if (move.typeOf() == Move::CASTLING && move.to() < move.from()) {
                return move;
            }
        }

        throw std::runtime_error("Illegal San, Step 2: " + san);
    }

    // A move looks like this:

    // [NBKRQ]? ([a-h])? ([1-8])? x? [a-h] [1-8] (=[nbrqkNBRQK])?

    if (san.size() < 2) {
        throw std::runtime_error("Illegal San, Step 3: " + san);
    }

    PieceType pt = PieceType::NONE;

    PieceType promotion = PieceType::NONE;

    File file_from = File::NO_FILE;
    Rank rank_from = Rank::NO_RANK;

    File file_to = File::NO_FILE;
    Rank rank_to = Rank::NO_RANK;

    // check if san starts with file
    if (san[0] == 'N' || san[0] == 'B' || san[0] == 'R' || san[0] == 'Q' || san[0] == 'K') {
        pt = charToPieceType[san[0]];
        san.erase(0, 1);
    }

    if (san[0] >= 'a' && san[0] <= 'h') {
        file_from = File(int(san[0] - 97));

        if (pt == PieceType::NONE) {
            pt = PieceType::PAWN;
        }

        san.erase(0, 1);
    }

    if (san[0] >= '1' && san[0] <= '8') {
        rank_from = Rank(int(san[0] - 49));

        san.erase(0, 1);
    }

    // skip capture sign
    if (san[0] == 'x') {
        san.erase(0, 1);
    }

    if (san[0] >= 'a' && san[0] <= 'h') {
        file_to = File(int(san[0] - 97));

        if (pt == PieceType::NONE) {
            pt = PieceType::PAWN;
        }

        san.erase(0, 1);
    }

    if (san[0] >= '1' && san[0] <= '8') {
        rank_to = Rank(int(san[0] - 49));

        san.erase(0, 1);
    }

    if (san[0] == '=') {
        san.erase(0, 1);

        promotion = charToPieceType[san[0]];
    }

    // the from square is actually the to
    if (file_to == File::NO_FILE && rank_to == Rank::NO_RANK) {
        file_to   = file_from;
        rank_to   = rank_from;
        file_from = File::NO_FILE;
        rank_from = Rank::NO_RANK;
    }

    Square from_sq = NO_SQ;
    Square to_sq   = utils::fileRankSquare(file_to, rank_to);

    if (file_from != File::NO_FILE && rank_from != Rank::NO_RANK) {
        from_sq = utils::fileRankSquare(file_from, rank_from);
    }

    for (const auto move : moves) {
        if (pt != board.at<PieceType>(move.from()) || move.to() != to_sq) {
            continue;
        }

        if (promotion != PieceType::NONE && move.typeOf() == Move::PROMOTION &&
            promotion == move.promotionType() && move.to() == to_sq &&
            ((file_from == File::NO_FILE && utils::squareFile(move.from()) == file_to) ||
             utils::squareFile(move.from()) == file_from)) {
            return move;
        }

        if (move.typeOf() == Move::PROMOTION) continue;

        if (move.typeOf() == Move::ENPASSANT && pt == PieceType::PAWN && move.to() == to_sq &&
            utils::squareFile(move.from()) == file_from) {
            return move;
        }

        if (move.typeOf() == Move::ENPASSANT) continue;

        if (rank_from == Rank::NO_RANK && file_from == File::NO_FILE) {
            return move;
        }

        if (from_sq != NO_SQ) {
            if (move.from() == from_sq) {
                return move;
            }
            continue;
        }

        if ((utils::squareFile(move.from()) == file_from) ||
            (utils::squareRank(move.from()) == rank_from)) {
            return move;
        }
    }

    std::cout << "pt " << int(pt) << std::endl;
    std::cout << "file_from " << int(file_from) << std::endl;
    std::cout << "rank_from " << int(rank_from) << std::endl;
    std::cout << "file_to " << int(file_to) << std::endl;
    std::cout << "rank_to " << int(rank_to) << std::endl;
    std::cout << "promotion " << int(promotion) << std::endl;
    std::cout << "to_sq " << squareToString[int(to_sq)] << std::endl;

    throw std::runtime_error("Illegal San, Step 4: " + original + " " + board.getFen());
}

}  // namespace uci

/// @brief Object representing a chess game
struct Game {
   public:
    Game() = default;

    Game(const std::unordered_map<std::string, std::string> &headers,
         const std::vector<PgnMove> &moves)
        : headers_(headers), moves_(moves) {}

    /// @brief Get the headers of the game
    /// @return
    [[nodiscard]] const std::unordered_map<std::string, std::string> &headers() const {
        return headers_;
    }

    /// @brief Get the moves of the game
    [[nodiscard]] const std::vector<PgnMove> &moves() const { return moves_; }
    [[nodiscard]] std::vector<PgnMove> &moves() { return moves_; }

    /// @brief Set a header
    /// @param key
    /// @param value
    void setHeader(const std::string &key, const std::string &value) { headers_[key] = value; }

   private:
    std::unordered_map<std::string, std::string> headers_;
    std::vector<PgnMove> moves_;
};

namespace pgn {

/// @brief [Internal use only]
/// @param line
/// @return
inline std::pair<std::string, std::string> extractHeader(const std::string &line) {
    std::string key;
    std::string value;

    bool readingKey   = false;
    bool readingValue = false;

    for (const auto c : line) {
        if (c == '[') {
            readingKey = true;
        } else if (c == '"') {
            readingValue = !readingValue;
        } else if (readingKey && c == ' ') {
            readingKey = false;
        } else if (readingKey) {
            key += c;
        } else if (readingValue) {
            value += c;
        }
    }

    return {key, value};
}

/// @brief [Internal use only] Extract and parse the move, plus any comments it might have.
/// @param board
/// @param line
/// @return
inline void extractMoves(Board &board, std::vector<PgnMove> &moves, std::string_view line) {
    std::string move;
    std::string comment;

    bool readingMove    = false;
    bool readingComment = false;

    // Pgn are build up in the following way.
    // {move_number} {move} {comment} {move} {comment} {move_number} ...
    // So we need to skip the move_number then start reading the move, then save the comment
    // then read the second move in the group. After that a move_number will follow again.
    for (const auto c : line) {
        if (readingMove && c == ' ') {
            readingMove = false;
        } else if (readingMove) {
            move += c;
        } else if (!readingComment && c == '{') {
            readingComment = true;
        } else if (readingComment && c == '}') {
            readingComment = false;

            if (!move.empty()) {
                const auto move_internal = uci::parseSan(board, move);
                moves.push_back({move_internal, comment});

                board.makeMove(move_internal);

                move.clear();
                comment.clear();
            }
        } else if (!readingMove && !readingComment) {
            if (!std::isalpha(c)) {
                continue;
            }

            if (!move.empty()) {
                const auto move_internal = uci::parseSan(board, move);
                moves.push_back({move_internal, comment});

                board.makeMove(move_internal);

                move.clear();
                comment.clear();
            }

            readingMove = true;
            move += c;
        } else if (readingComment) {
            comment += c;
        }
    }

    // add the remaining move
    if (!move.empty()) {
        const auto move_internal = uci::parseSan(board, move);
        moves.push_back({move_internal, comment});

        board.makeMove(move_internal);

        move.clear();
        comment.clear();
    }
}

/// @brief Read the next game from a file
/// @param file
/// @return
inline std::optional<Game> readGame(std::ifstream &file) {
    Board board = Board();

    Game game;

    std::string line;

    bool readingMoves = false;

    bool hasHead = false;
    bool hasBody = false;

    while (!utils::safeGetline(file, line).eof()) {
        // We read the moves and we reached the end of the pgn, which is signaled by an empty line.
        if (readingMoves && line.empty()) {
            break;
        }

        if (line[0] == '[') {
            // Parse the header
            const auto header = extractHeader(line);

            hasHead = true;

            game.setHeader(header.first, header.second);

            if (header.first == "FEN") {
                board.setFen(header.second);
            }

            if (header.first == "Variant") {
                board.set960(header.second == "fischerandom");
            }
        } else {
            // Parse the moves
            extractMoves(board, game.moves(), line);

            readingMoves = true;
            hasBody      = true;
        }
    }

    if (!hasBody && !hasHead) {
        return std::nullopt;
    }

    return game;
}

}  // namespace pgn

}  // namespace chess

#endif  // CHESS_HPP
