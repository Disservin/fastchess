#pragma once

#include "types.h"

struct Move
{
    Square from_sq;
    Square to_sq;
    PieceType promotion_piece;
};