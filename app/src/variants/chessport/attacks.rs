use crate::variants::chessport::bitboard::Bitboard;
use crate::variants::chessport::color::Color;
use crate::variants::chessport::coords::{Direction, File, Rank, Square};

// ---------------------------------------------------------------------------
// File / Rank masks (computed on-the-fly)
// ---------------------------------------------------------------------------

#[inline(always)]
pub fn mask_rank(rank: Rank) -> Bitboard {
    Bitboard(0xff_u64 << (8 * rank.as_u8()))
}

#[inline(always)]
pub fn mask_file(file: File) -> Bitboard {
    Bitboard(0x0101010101010101u64 << file.as_u8())
}

// ---------------------------------------------------------------------------
// Pawn attacks (computed on-the-fly)
// ---------------------------------------------------------------------------

#[inline(always)]
pub fn pawn(c: Color, sq: Square) -> Bitboard {
    let bb = Bitboard::from_square(sq);
    if c == Color::White {
        // White pawns attack diagonally up-left and up-right
        let left = (bb.0 << 7) & !mask_file(File::FileH).0;
        let right = (bb.0 << 9) & !mask_file(File::FileA).0;
        Bitboard(left | right)
    } else {
        // Black pawns attack diagonally down-left and down-right
        let left = (bb.0 >> 9) & !mask_file(File::FileH).0;
        let right = (bb.0 >> 7) & !mask_file(File::FileA).0;
        Bitboard(left | right)
    }
}

/// Pawn left-side attacks for the given color.
#[inline(always)]
pub fn pawn_left_attacks(pawns: Bitboard, c: Color) -> Bitboard {
    if c == Color::White {
        Bitboard((pawns.0 << 7) & !mask_file(File::FileH).0)
    } else {
        Bitboard((pawns.0 >> 7) & !mask_file(File::FileA).0)
    }
}

/// Pawn right-side attacks for the given color.
#[inline(always)]
pub fn pawn_right_attacks(pawns: Bitboard, c: Color) -> Bitboard {
    if c == Color::White {
        Bitboard((pawns.0 << 9) & !mask_file(File::FileA).0)
    } else {
        Bitboard((pawns.0 >> 9) & !mask_file(File::FileH).0)
    }
}

// ---------------------------------------------------------------------------
// Knight attacks (computed on-the-fly)
// ---------------------------------------------------------------------------

const KNIGHT_OFFSETS: [(i32, i32); 8] = [
    (2, 1),
    (2, -1),
    (-2, 1),
    (-2, -1),
    (1, 2),
    (1, -2),
    (-1, 2),
    (-1, -2),
];

#[inline(always)]
pub fn knight(sq: Square) -> Bitboard {
    let file = sq.file().as_u8() as i32;
    let rank = sq.rank().as_u8() as i32;
    let mut attacks = Bitboard(0);

    for (df, dr) in KNIGHT_OFFSETS {
        let new_file = file + df;
        let new_rank = rank + dr;
        if new_file >= 0 && new_file <= 7 && new_rank >= 0 && new_rank <= 7 {
            attacks.set((new_rank * 8 + new_file) as u8);
        }
    }
    attacks
}

// ---------------------------------------------------------------------------
// King attacks (computed on-the-fly)
// ---------------------------------------------------------------------------

const KING_OFFSETS: [(i32, i32); 8] = [
    (0, 1),
    (0, -1),
    (1, 0),
    (-1, 0),
    (1, 1),
    (1, -1),
    (-1, 1),
    (-1, -1),
];

#[inline(always)]
pub fn king(sq: Square) -> Bitboard {
    let file = sq.file().as_u8() as i32;
    let rank = sq.rank().as_u8() as i32;
    let mut attacks = Bitboard(0);

    for (df, dr) in KING_OFFSETS {
        let new_file = file + df;
        let new_rank = rank + dr;
        if new_file >= 0 && new_file <= 7 && new_rank >= 0 && new_rank <= 7 {
            attacks.set((new_rank * 8 + new_file) as u8);
        }
    }
    attacks
}

// ---------------------------------------------------------------------------
// Sliding piece attacks (computed on-the-fly, no lookup tables)
// ---------------------------------------------------------------------------

const BISHOP_DIRECTIONS: [(i32, i32); 4] = [(1, 1), (1, -1), (-1, -1), (-1, 1)];
const ROOK_DIRECTIONS: [(i32, i32); 4] = [(0, 1), (1, 0), (0, -1), (-1, 0)];

#[inline(always)]
fn sliding_attacks(sq: Square, occupied: Bitboard, directions: &[(i32, i32); 4]) -> Bitboard {
    let file = sq.file().as_u8() as i32;
    let rank = sq.rank().as_u8() as i32;
    let mut attacks = Bitboard(0);

    for (df, dr) in directions {
        let mut f = file + df;
        let mut r = rank + dr;
        while f >= 0 && f <= 7 && r >= 0 && r <= 7 {
            let sq_idx = (r * 8 + f) as u8;
            attacks.set(sq_idx);
            if occupied.check(sq_idx) {
                break;
            }
            f += df;
            r += dr;
        }
    }
    attacks
}

#[inline(always)]
pub fn bishop(sq: Square, occupied: Bitboard) -> Bitboard {
    sliding_attacks(sq, occupied, &BISHOP_DIRECTIONS)
}

#[inline(always)]
pub fn rook(sq: Square, occupied: Bitboard) -> Bitboard {
    sliding_attacks(sq, occupied, &ROOK_DIRECTIONS)
}

#[inline(always)]
pub fn queen(sq: Square, occupied: Bitboard) -> Bitboard {
    bishop(sq, occupied) | rook(sq, occupied)
}

// ---------------------------------------------------------------------------
// Shift function
// ---------------------------------------------------------------------------

/// Shift a bitboard in a given direction.
#[inline(always)]
pub fn shift(b: Bitboard, dir: Direction) -> Bitboard {
    match dir {
        Direction::North => Bitboard(b.0 << 8),
        Direction::South => Bitboard(b.0 >> 8),
        Direction::NorthWest => Bitboard((b.0 & !mask_file(File::FileA).0) << 7),
        Direction::West => Bitboard((b.0 & !mask_file(File::FileA).0) >> 1),
        Direction::SouthWest => Bitboard((b.0 & !mask_file(File::FileA).0) >> 9),
        Direction::NorthEast => Bitboard((b.0 & !mask_file(File::FileH).0) << 9),
        Direction::East => Bitboard((b.0 & !mask_file(File::FileH).0) << 1),
        Direction::SouthEast => Bitboard((b.0 & !mask_file(File::FileH).0) >> 7),
    }
}

// ---------------------------------------------------------------------------
// Initialization (no-op now, kept for API compatibility)
// ---------------------------------------------------------------------------

pub fn init_attacks() {
    // No initialization needed - everything is computed on-the-fly
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::coords::Square;

    #[test]
    fn rook_a1_empty_board() {
        // Rook on A1 (sq=0) with empty board: should see all of file A and rank 1
        let sq = Square(0); // A1
        let atk = rook(sq, Bitboard(0));
        // File A = 0x0101010101010100 (all of A except A1 itself)
        // Rank 1 = 0x00000000000000FE (all of rank 1 except A1)
        let expected = Bitboard(0x01010101010101FE);
        assert_eq!(
            atk, expected,
            "Rook A1 empty board mismatch: got {:016x}",
            atk.0
        );
    }

    #[test]
    fn rook_e4_empty_board() {
        // Rook on E4 (sq=28): should see full rank 4 and full file E minus the square itself
        let sq = Square(28); // E4
        let atk = rook(sq, Bitboard(0));
        // File E = col 4 = 0x1010101010101010, Rank 4 = 0x00000000FF000000
        // Union minus E4 bit
        let file_e: u64 = 0x1010101010101010;
        let rank_4: u64 = 0x00000000FF000000;
        let expected = Bitboard((file_e | rank_4) & !(1u64 << 28));
        assert_eq!(
            atk, expected,
            "Rook E4 empty board mismatch: got {:016x} want {:016x}",
            atk.0, expected.0
        );
    }

    #[test]
    fn bishop_d4_empty_board() {
        // Bishop on D4 (sq=27): should see both diagonals
        let sq = Square(27); // D4
        let atk = bishop(sq, Bitboard(0));
        // Manually compute both diagonals
        let mut expected: u64 = 0;
        let (pf, pr) = (3i32, 3i32); // D=3, rank4=3
        for &(df, dr) in &[(1i32, 1i32), (1, -1), (-1, -1), (-1, 1)] {
            let (mut f, mut r) = (pf + df, pr + dr);
            while f >= 0 && f <= 7 && r >= 0 && r <= 7 {
                expected |= 1u64 << (r * 8 + f);
                f += df;
                r += dr;
            }
        }
        assert_eq!(
            atk,
            Bitboard(expected),
            "Bishop D4 empty board mismatch: got {:016x} want {:016x}",
            atk.0,
            expected
        );
    }

    #[test]
    fn knight_e4() {
        let sq = Square(28); // E4
        let atk = knight(sq);
        // Knight on E4: should reach D2, F2, C3, G3, C5, G5, D6, F6
        let expected_squares: &[u8] = &[11, 13, 18, 22, 34, 38, 43, 45];
        let mut expected: u64 = 0;
        for &s in expected_squares {
            expected |= 1u64 << s;
        }
        assert_eq!(atk, Bitboard(expected), "Knight E4 mismatch");
    }

    #[test]
    fn king_e4() {
        let sq = Square(28); // E4
        let atk = king(sq);
        // King on E4: D3,E3,F3,D4,F4,D5,E5,F5
        let expected_squares: &[u8] = &[19, 20, 21, 27, 29, 35, 36, 37];
        let mut expected: u64 = 0;
        for &s in expected_squares {
            expected |= 1u64 << s;
        }
        assert_eq!(atk, Bitboard(expected), "King E4 mismatch");
    }

    #[test]
    fn pawn_attacks_white_e4() {
        let sq = Square(28); // E4
        let atk = pawn(Color::White, sq);
        // White pawn on E4 attacks D5 (35) and F5 (37)
        let expected = Bitboard((1u64 << 35) | (1u64 << 37));
        assert_eq!(atk, expected, "White pawn E4 mismatch");
    }

    #[test]
    fn pawn_attacks_black_e5() {
        let sq = Square(36); // E5
        let atk = pawn(Color::Black, sq);
        // Black pawn on E5 attacks D4 (27) and F4 (29)
        let expected = Bitboard((1u64 << 27) | (1u64 << 29));
        assert_eq!(atk, expected, "Black pawn E5 mismatch");
    }
}
