/*
 * (c) 2015 basil, all rights reserved,
 * Modifications Copyright (c) 2016-2019 by Jon Dart
 * Modifications Copyright (c) 2020-2024 by Andrew Grant
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

/*
 * You are in charge of defining each of these macros. The macros already
 * defined here are simply an example of what to do. This configuration is
 * used by Ethereal to implement Pyrrhic.
 *
 * See Ethereal's source <https://github.com/AndyGrant/Ethereal> if it is
 * not readily clear what these definfitions mean. The relevant files are
 * are the ones included below.
 *
 * Note that for the Pawn Attacks, we invert the colour. This is because
 * Pyrrhic defines White as 1, where as Ethereal (any many others) choose
 * to define White as 0 and Black as 1.
 */

#include <chess.hpp>

inline int poplsb(uint64_t* x) {
    const int lsb = chess::Bitboard(*x).lsb();
    *x &= *x - 1;
    return lsb;
}

#define PYRRHIC_POPCOUNT(x) (chess::Bitboard(x).count())
#define PYRRHIC_LSB(x) (chess::Bitboard(x).lsb())
#define PYRRHIC_POPLSB(x) (poplsb(x))

#define PYRRHIC_PAWN_ATTACKS(sq, c) (chess::attacks::pawn((chess::Color)!c, (chess::Square)(sq)).getBits())
#define PYRRHIC_KNIGHT_ATTACKS(sq) (chess::attacks::knight((chess::Square)(sq)).getBits())
#define PYRRHIC_BISHOP_ATTACKS(sq, occ) (chess::attacks::bishop((chess::Square)(sq), (chess::Bitboard)(occ)).getBits())
#define PYRRHIC_ROOK_ATTACKS(sq, occ) (chess::attacks::rook((chess::Square)(sq), (chess::Bitboard)(occ)).getBits())
#define PYRRHIC_QUEEN_ATTACKS(sq, occ) (chess::attacks::queen((chess::Square)(sq), (chess::Bitboard)(occ)).getBits())
#define PYRRHIC_KING_ATTACKS(sq) (chess::attacks::king((chess::Square)(sq)).getBits())
