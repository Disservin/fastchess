#pragma once

#include <array>

#include "types.h"

Square lsb(Bitboard mask);

Square msb(Bitboard mask);

int popcount(Bitboard mask);

Square poplsb(Bitboard &mask);

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

    void reset(T value)
    {
        data.fill({value});
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

    void reset(T value)
    {
        data.fill({value});
    }
};
