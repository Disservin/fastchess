#pragma once

#include "engine.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <vector>

#include "types.h"

bool starts_with(std::string_view haystack, std::string_view needle);

template<typename T>
T findElement(const std::vector<std::string>& haystack, std::string_view needle)
{
	int index = std::find(haystack.begin(), haystack.end(), needle) - haystack.begin();
	if constexpr (std::is_same_v<T, int>)
		return std::stoi(haystack[index + 1]);
	else
		return haystack[index + 1];
}

bool contains(std::string_view haystack, std::string_view needle);

bool contains(const std::vector<std::string>& haystack, std::string_view needle);

std::vector<std::string> splitString(const std::string& string, const char& delimiter);

/// @brief Table template class for creating N-dimensional arrays.
/// @tparam T
/// @tparam N
/// @tparam ...Dims
template<typename T, size_t N, size_t... Dims>
struct Table
{
	std::array<Table<T, Dims...>, N> data;

	Table()
	{
		data.fill({});
	}

	Table<T, Dims...>& operator[](size_t index)
	{
		return data[index];
	}

	const Table<T, Dims...>& operator[](size_t index) const
	{
		return data[index];
	}

	void reset(T value)
	{
		data.fill({ value });
	}
};

template<typename T, size_t N>
struct Table<T, N>
{
	std::array<T, N> data;

	Table()
	{
		data.fill({});
	}

	T& operator[](size_t index)
	{
		return data[index];
	}

	const T& operator[](size_t index) const
	{
		return data[index];
	}

	void reset(T value)
	{
		data.fill({ value });
	}
};

Square lsb(Bitboard mask);

Square msb(Bitboard mask);

int popcount(Bitboard mask);

Square poplsb(Bitboard& mask);

inline constexpr PieceType type_of_piece(const Piece piece)
{
	constexpr PieceType PieceToPieceType[13] = { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN,
												 KNIGHT, BISHOP, ROOK, QUEEN, KING, NONETYPE };
	return PieceToPieceType[piece];
}

inline constexpr File square_file(Square sq)
{
	return File(sq & 7);
}

inline constexpr Rank square_rank(Square sq)
{
	return Rank(sq >> 3);
}

inline constexpr Square file_rank_square(File f, Rank r)
{
	return Square((r << 3) + f);
}

inline Piece make_piece(PieceType type, Color c)
{
	if (type == NONETYPE)
		return NONE;
	return Piece(type + 6 * c);
}

// returns diagonal of given square
inline constexpr uint8_t diagonal_of(Square sq)
{
	return 7 + square_rank(sq) - square_file(sq);
}

// returns anti diagonal of given square
inline constexpr uint8_t anti_diagonal_of(Square sq)
{
	return square_rank(sq) + square_file(sq);
}
