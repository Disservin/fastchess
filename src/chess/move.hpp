#pragma once

#include <iostream>

#include "types.hpp"

struct Move
{
    Square from_sq = NO_SQ;
    Square to_sq = NO_SQ;
    PieceType moving_piece = NONETYPE;
    PieceType promotion_piece = NONETYPE;

    Move() = default;

    Move(Square from_sq_cp, Square to_sq_cp, PieceType _moving_piece, PieceType promotion_piece_cp)
        : from_sq(from_sq_cp), to_sq(to_sq_cp), moving_piece(_moving_piece), promotion_piece(promotion_piece_cp)
    {
    }
};

inline std::ostream &operator<<(std::ostream &os, const Move &m)
{
    os << "from sq " << int(m.from_sq) << " to_sq " << int(m.to_sq) << " promotion " << int(m.promotion_piece);
    return os;
}

inline bool operator==(const Move &lhs, const Move &rhs)
{
    return lhs.from_sq == rhs.from_sq && lhs.to_sq == rhs.to_sq && lhs.promotion_piece == rhs.promotion_piece;
}

inline bool operator!=(const Move &lhs, const Move &rhs)
{
    return !(lhs == rhs);
}
