use std::fmt;
use std::ops::{BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Not, Shl, Shr};

use crate::variants::chessport::coords::{File, Rank, Square};

#[derive(Copy, Clone, Default, PartialEq, Eq)]
pub struct Bitboard(pub u64);

impl Bitboard {
    pub const EMPTY: Bitboard = Bitboard(0);

    #[inline(always)]
    pub const fn new(bits: u64) -> Self {
        Bitboard(bits)
    }

    /// Construct a Bitboard with the file's column set.
    pub fn from_file(file: File) -> Self {
        debug_assert!(file != File::NO_FILE);
        Bitboard(0x0101010101010101u64 << (file as u64))
    }

    /// Construct a Bitboard with the rank's row set.
    pub fn from_rank(rank: Rank) -> Self {
        debug_assert!(rank != Rank::NO_RANK);
        Bitboard(0xFFu64 << (8 * rank as u64))
    }

    /// Construct a Bitboard with a single square set.
    #[inline(always)]
    pub const fn from_square(sq: Square) -> Self {
        debug_assert!((sq.0 as usize) < 64);
        Bitboard(1u64 << sq.0)
    }

    /// Construct a Bitboard with a single square index set.
    #[inline(always)]
    pub const fn from_index(index: u8) -> Self {
        debug_assert!((index as usize) < 64);
        Bitboard(1u64 << index)
    }

    #[inline(always)]
    pub const fn is_empty(self) -> bool {
        self.0 == 0
    }

    #[inline(always)]
    pub const fn get_bits(self) -> u64 {
        self.0
    }

    /// Set a bit at `index`.
    #[inline(always)]
    pub fn set(&mut self, index: u8) {
        debug_assert!((index as usize) < 64);
        self.0 |= 1u64 << index;
    }

    /// Clear a bit at `index`.
    #[inline(always)]
    pub fn clear_bit(&mut self, index: u8) {
        debug_assert!((index as usize) < 64);
        self.0 &= !(1u64 << index);
    }

    /// Clear all bits.
    #[inline(always)]
    pub fn clear(&mut self) {
        self.0 = 0;
    }

    /// Check whether bit at `index` is set.
    #[inline(always)]
    pub const fn check(self, index: u8) -> bool {
        self.0 & (1u64 << index) != 0
    }

    /// Index of the least-significant set bit.
    #[inline(always)]
    pub fn lsb(self) -> u8 {
        debug_assert!(self.0 != 0);
        self.0.trailing_zeros() as u8
    }

    /// Index of the most-significant set bit.
    #[inline(always)]
    pub fn msb(self) -> u8 {
        debug_assert!(self.0 != 0);
        (63 ^ self.0.leading_zeros()) as u8
    }

    /// Population count (number of set bits).
    #[inline(always)]
    pub fn count(self) -> u32 {
        self.0.count_ones()
    }

    /// Pop the least-significant bit, returning its index.
    #[inline(always)]
    pub fn pop(&mut self) -> u8 {
        debug_assert!(self.0 != 0);
        let index = self.0.trailing_zeros() as u8;
        self.0 &= self.0 - 1;
        index
    }
}

// ---- bool conversion ----

impl From<Bitboard> for bool {
    #[inline(always)]
    fn from(b: Bitboard) -> bool {
        b.0 != 0
    }
}

// ---- operators between Bitboard and Bitboard ----

impl BitAnd for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn bitand(self, rhs: Bitboard) -> Bitboard {
        Bitboard(self.0 & rhs.0)
    }
}

impl BitOr for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn bitor(self, rhs: Bitboard) -> Bitboard {
        Bitboard(self.0 | rhs.0)
    }
}

impl BitXor for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn bitxor(self, rhs: Bitboard) -> Bitboard {
        Bitboard(self.0 ^ rhs.0)
    }
}

impl Not for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn not(self) -> Bitboard {
        Bitboard(!self.0)
    }
}

impl BitAndAssign for Bitboard {
    #[inline(always)]
    fn bitand_assign(&mut self, rhs: Bitboard) {
        self.0 &= rhs.0;
    }
}

impl BitOrAssign for Bitboard {
    #[inline(always)]
    fn bitor_assign(&mut self, rhs: Bitboard) {
        self.0 |= rhs.0;
    }
}

impl BitXorAssign for Bitboard {
    #[inline(always)]
    fn bitxor_assign(&mut self, rhs: Bitboard) {
        self.0 ^= rhs.0;
    }
}

// ---- operators between Bitboard and u64 ----

impl BitAnd<u64> for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn bitand(self, rhs: u64) -> Bitboard {
        Bitboard(self.0 & rhs)
    }
}

impl BitOr<u64> for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn bitor(self, rhs: u64) -> Bitboard {
        Bitboard(self.0 | rhs)
    }
}

impl BitXor<u64> for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn bitxor(self, rhs: u64) -> Bitboard {
        Bitboard(self.0 ^ rhs)
    }
}

impl BitAnd<Bitboard> for u64 {
    type Output = Bitboard;
    #[inline(always)]
    fn bitand(self, rhs: Bitboard) -> Bitboard {
        Bitboard(self & rhs.0)
    }
}

impl BitOr<Bitboard> for u64 {
    type Output = Bitboard;
    #[inline(always)]
    fn bitor(self, rhs: Bitboard) -> Bitboard {
        Bitboard(self | rhs.0)
    }
}

impl Shl<u64> for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn shl(self, rhs: u64) -> Bitboard {
        Bitboard(self.0 << rhs)
    }
}

impl Shr<u64> for Bitboard {
    type Output = Bitboard;
    #[inline(always)]
    fn shr(self, rhs: u64) -> Bitboard {
        Bitboard(self.0 >> rhs)
    }
}

impl PartialEq<u64> for Bitboard {
    fn eq(&self, other: &u64) -> bool {
        self.0 == *other
    }
}

// ---- Display: show the board as 8 rows of 8 bits ----

impl fmt::Display for Bitboard {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let b = self.0;
        for rank in (0..8).rev() {
            for file in 0..8 {
                let sq = rank * 8 + file;
                let c = if b & (1u64 << sq) != 0 { '1' } else { '0' };
                write!(f, "{}", c)?;
            }
            writeln!(f)?;
        }
        Ok(())
    }
}

impl fmt::Debug for Bitboard {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::coords::Square;

    #[test]
    fn lsb() {
        let b = Bitboard(0x0000000000000001u64);
        assert_eq!(b.lsb(), 0);

        let b = Bitboard(0x0000000000000002u64);
        assert_eq!(b.lsb(), 1);

        let b = Bitboard(0x0000000000000004u64);
        assert_eq!(b.lsb(), 2);
    }

    #[test]
    fn msb() {
        let b = Bitboard(0x8000000000000000u64);
        assert_eq!(b.msb(), 63);

        let b = Bitboard(0x4000000000000000u64);
        assert_eq!(b.msb(), 62);

        let b = Bitboard(0x2000000000000000u64);
        assert_eq!(b.msb(), 61);
    }

    #[test]
    fn popcount() {
        let b = Bitboard(0x0000000000000001u64);
        assert_eq!(b.count(), 1);

        let b = Bitboard(0x0000000000000003u64);
        assert_eq!(b.count(), 2);

        let b = Bitboard(0x0000000000000007u64);
        assert_eq!(b.count(), 3);
    }

    #[test]
    fn pop() {
        let mut b = Bitboard(0x0000000000000001u64);
        assert_eq!(b.pop(), 0);
        assert_eq!(b, 0u64);

        let mut b = Bitboard(0x0000000000000003u64);
        assert_eq!(b.pop(), 0);
        assert_eq!(b, 0x0000000000000002u64);

        let mut b = Bitboard(0x0000000000000007u64);
        assert_eq!(b.pop(), 0);
        assert_eq!(b, 0x0000000000000006u64);
    }

    #[test]
    fn get_bits() {
        assert_eq!(
            Bitboard(0x0000000000000001u64).get_bits(),
            0x0000000000000001u64
        );
        assert_eq!(
            Bitboard(0x0000000000000003u64).get_bits(),
            0x0000000000000003u64
        );
        assert_eq!(
            Bitboard(0x0000000000000007u64).get_bits(),
            0x0000000000000007u64
        );
    }

    #[test]
    fn empty() {
        let b = Bitboard(0x0000000000000000u64);
        assert!(b.is_empty());

        let b = Bitboard(0x0000000000000001u64);
        assert!(!b.is_empty());

        let b = Bitboard(0x0000000000000003u64);
        assert!(!b.is_empty());
    }

    #[test]
    fn op_eq() {
        assert_eq!(Bitboard(0u64), 0u64);
        assert_eq!(Bitboard(0x0000000000000001u64), 0x0000000000000001u64);
        assert_eq!(Bitboard(0x0000000000000003u64), 0x0000000000000003u64);
    }

    #[test]
    fn op_ne() {
        assert_ne!(Bitboard(0u64), 0x0000000000000001u64);
        assert_ne!(Bitboard(0x0000000000000001u64), 0x0000000000000002u64);
        assert_ne!(Bitboard(0x0000000000000003u64), 0x0000000000000004u64);
    }

    #[test]
    fn op_and() {
        assert_eq!((Bitboard(0u64) & 0u64), 0u64);
        assert_eq!(
            (Bitboard(0x0000000000000001u64) & 0x0000000000000001u64),
            0x0000000000000001u64
        );
        assert_eq!(
            (Bitboard(0x0000000000000003u64) & 0x0000000000000003u64),
            0x0000000000000003u64
        );
    }

    #[test]
    fn op_or() {
        assert_eq!((Bitboard(0u64) | 0u64), 0u64);
        assert_eq!(
            (Bitboard(0x0000000000000001u64) | 0x0000000000000001u64),
            0x0000000000000001u64
        );
        assert_eq!(
            (Bitboard(0x0000000000000003u64) | 0x0000000000000003u64),
            0x0000000000000003u64
        );
    }

    #[test]
    fn op_xor() {
        assert_eq!((Bitboard(0u64) ^ 0u64), 0u64);
        assert_eq!(
            (Bitboard(0x0000000000000001u64) ^ 0x0000000000000001u64),
            0u64
        );
        assert_eq!(
            (Bitboard(0x0000000000000003u64) ^ 0x0000000000000003u64),
            0u64
        );
    }

    #[test]
    fn op_shl() {
        assert_eq!((Bitboard(0u64) << 0u64), 0u64);
        assert_eq!(
            (Bitboard(0x0000000000000001u64) << 1u64),
            0x0000000000000002u64
        );
        assert_eq!(
            (Bitboard(0x0000000000000003u64) << 2u64),
            0x000000000000000cu64
        );
    }

    #[test]
    fn op_shr() {
        assert_eq!((Bitboard(0u64) >> 0u64), 0u64);
        assert_eq!((Bitboard(0x0000000000000001u64) >> 1u64), 0u64);
        assert_eq!((Bitboard(0x0000000000000003u64) >> 2u64), 0u64);
    }

    #[test]
    fn op_and_assign() {
        let mut b = Bitboard(0u64);
        b &= Bitboard(0u64);
        assert_eq!(b, 0u64);

        let mut b = Bitboard(0x0000000000000001u64);
        b &= Bitboard(0x0000000000000001u64);
        assert_eq!(b, 0x0000000000000001u64);

        let mut b = Bitboard(0x0000000000000003u64);
        b &= Bitboard(0x0000000000000003u64);
        assert_eq!(b, 0x0000000000000003u64);
    }

    #[test]
    fn op_or_assign() {
        let mut b = Bitboard(0u64);
        b |= Bitboard(0u64);
        assert_eq!(b, 0u64);

        let mut b = Bitboard(0x0000000000000001u64);
        b |= Bitboard(0x0000000000000001u64);
        assert_eq!(b, 0x0000000000000001u64);

        let mut b = Bitboard(0x0000000000000003u64);
        b |= Bitboard(0x0000000000000003u64);
        assert_eq!(b, 0x0000000000000003u64);
    }

    #[test]
    fn op_xor_assign() {
        let mut b = Bitboard(0u64);
        b ^= Bitboard(0u64);
        assert_eq!(b, 0u64);

        let mut b = Bitboard(0x0000000000000001u64);
        b ^= Bitboard(0x0000000000000001u64);
        assert_eq!(b, 0u64);

        let mut b = Bitboard(0x0000000000000003u64);
        b ^= Bitboard(0x0000000000000003u64);
        assert_eq!(b, 0u64);
    }

    #[test]
    fn op_not() {
        assert_eq!(!Bitboard(0u64), 0xffffffffffffffffu64);
        assert_eq!(!Bitboard(0x0000000000000001u64), 0xfffffffffffffffeu64);
        assert_eq!(!Bitboard(0x0000000000000003u64), 0xfffffffffffffffcu64);
    }

    #[test]
    fn set_bit() {
        let mut b = Bitboard(0u64);
        b.set(0);
        assert_eq!(b, 0x0000000000000001u64);
        b.set(1);
        assert_eq!(b, 0x0000000000000003u64);
        b.set(2);
        assert_eq!(b, 0x0000000000000007u64);
    }

    #[test]
    fn check_bit() {
        let mut b = Bitboard(0u64);
        assert!(!b.check(0));
        b.set(0);
        assert!(b.check(0));
        b.set(1);
        assert!(b.check(1));
    }

    #[test]
    fn clear_bit() {
        let mut b = Bitboard(0u64);
        b.clear_bit(0);
        assert_eq!(b, 0u64);

        b.set(0);
        b.clear_bit(0);
        assert_eq!(b, 0u64);

        b.set(1);
        b.clear_bit(1);
        assert_eq!(b, 0u64);
    }

    #[test]
    fn clear_all() {
        let mut b = Bitboard(0u64);
        b.clear();
        assert_eq!(b, 0u64);

        b.set(0);
        b.clear();
        assert_eq!(b, 0u64);

        b.set(1);
        b.clear();
        assert_eq!(b, 0u64);
    }

    #[test]
    fn from_square_index() {
        let b = Bitboard::from_index(0);
        assert_eq!(b, 0x0000000000000001u64);

        let b = Bitboard::from_index(1);
        assert_eq!(b, 0x0000000000000002u64);

        let b = Bitboard::from_index(2);
        assert_eq!(b, 0x0000000000000004u64);
    }

    #[test]
    fn display_bitboard() {
        // The C++ "operator string" test just compared str == str (trivially true).
        // Here we verify the Display output for a known position.
        let b = Bitboard::from_square(Square::SQ_A1);
        let s = format!("{}", b);
        // A1 is index 0. In the Display, rank 7 (top) is printed first.
        // Rank 0 (bottom) is the last row: bit 0 is file A = leftmost,
        // so the last row should be "10000000\n"
        assert!(s.ends_with("10000000\n"));
    }
}
