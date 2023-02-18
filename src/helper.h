#pragma once

#include "types.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <vector>

bool starts_with(std::string_view haystack, std::string_view needle);

template <typename T> T findElement(const std::vector<std::string> &haystack, std::string_view needle)
{
    int index = std::find(haystack.begin(), haystack.end(), needle) - haystack.begin();
    if constexpr (std::is_same_v<T, int>)
        return std::stoi(haystack[index + 1]);
    else
        return haystack[index + 1];
}

bool contains(std::string_view haystack, std::string_view needle);
bool contains(const std::vector<std::string> &haystack, std::string_view needle);
std::vector<std::string> splitString(const std::string &string, const char &delimiter);

/// @brief Table template class for creating N-dimensional arrays.
/// @tparam T
/// @tparam N
/// @tparam ...Dims
template <typename T, size_t N, size_t... Dims> struct Table
{
    std::array<Table<T, Dims...>, N> data;
    Table()
    {
        data.fill({});
    }
    Table<T, Dims...> &operator[](size_t index)
    {
        return data[index];
    }
    const Table<T, Dims...> &operator[](size_t index) const
    {
        return data[index];
    }

    void reset()
    {
        data.fill({});
    }
};

template <typename T, size_t N> struct Table<T, N>
{
    std::array<T, N> data;
    Table()
    {
        data.fill({});
    }
    T &operator[](size_t index)
    {
        return data[index];
    }
    const T &operator[](size_t index) const
    {
        return data[index];
    }

    void reset()
    {
        data.fill({});
    }
};

/// @brief least significant bit instruction
/// @param mask
/// @return the least significant bit as the Square
Square lsb(Bitboard mask);

/// @brief most significant bit instruction
/// @param mask
/// @return the most significant bit as the Square
Square msb(Bitboard mask);

/// @brief Counts the set bits
/// @param mask
/// @return the count
int popcount(Bitboard mask);

/// @brief remove the lsb and return it
/// @param mask
/// @return the lsb
Square poplsb(Bitboard &mask);

inline constexpr PieceType type_of_piece(const Piece piece)
{
    constexpr PieceType PieceToPieceType[13] = {PAWN,   KNIGHT, BISHOP, ROOK,  QUEEN, KING,    PAWN,
                                                KNIGHT, BISHOP, ROOK,   QUEEN, KING,  NONETYPE};
    return PieceToPieceType[piece];
}