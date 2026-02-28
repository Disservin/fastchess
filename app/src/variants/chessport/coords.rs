use crate::variants::chessport::color::Color;

// ---------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u8)]
pub enum File {
    FileA = 0,
    FileB = 1,
    FileC = 2,
    FileD = 3,
    FileE = 4,
    FileF = 5,
    FileG = 6,
    FileH = 7,
    NoFile = 8,
}

impl File {
    pub const FILE_A: File = File::FileA;
    pub const FILE_B: File = File::FileB;
    pub const FILE_C: File = File::FileC;
    pub const FILE_D: File = File::FileD;
    pub const FILE_E: File = File::FileE;
    pub const FILE_F: File = File::FileF;
    pub const FILE_G: File = File::FileG;
    pub const FILE_H: File = File::FileH;
    pub const NO_FILE: File = File::NoFile;

    #[inline]
    pub fn from_u8(v: u8) -> File {
        match v {
            0 => File::FileA,
            1 => File::FileB,
            2 => File::FileC,
            3 => File::FileD,
            4 => File::FileE,
            5 => File::FileF,
            6 => File::FileG,
            7 => File::FileH,
            _ => File::NoFile,
        }
    }

    /// Parse from a lowercase/uppercase character like 'a'–'h'.
    #[inline]
    pub fn from_char(c: char) -> File {
        let c = c.to_ascii_lowercase();
        if c >= 'a' && c <= 'h' {
            File::from_u8((c as u8) - b'a')
        } else {
            File::NoFile
        }
    }

    #[inline]
    pub fn as_u8(self) -> u8 {
        self as u8
    }

    #[inline]
    pub fn as_usize(self) -> usize {
        self as usize
    }
}

impl std::fmt::Display for File {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if *self == File::NoFile {
            write!(f, "-")
        } else {
            write!(f, "{}", (b'a' + self.as_u8()) as char)
        }
    }
}

impl std::ops::Add<i32> for File {
    type Output = File;
    #[inline]
    fn add(self, rhs: i32) -> File {
        File::from_u8((self.as_u8() as i32 + rhs) as u8)
    }
}

impl std::ops::Sub<File> for File {
    type Output = i32;
    #[inline]
    fn sub(self, rhs: File) -> i32 {
        self.as_u8() as i32 - rhs.as_u8() as i32
    }
}

// ---------------------------------------------------------------------------
// Rank
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u8)]
pub enum Rank {
    Rank1 = 0,
    Rank2 = 1,
    Rank3 = 2,
    Rank4 = 3,
    Rank5 = 4,
    Rank6 = 5,
    Rank7 = 6,
    Rank8 = 7,
    NoRank = 8,
}

impl Rank {
    pub const RANK_1: Rank = Rank::Rank1;
    pub const RANK_2: Rank = Rank::Rank2;
    pub const RANK_3: Rank = Rank::Rank3;
    pub const RANK_4: Rank = Rank::Rank4;
    pub const RANK_5: Rank = Rank::Rank5;
    pub const RANK_6: Rank = Rank::Rank6;
    pub const RANK_7: Rank = Rank::Rank7;
    pub const RANK_8: Rank = Rank::Rank8;
    pub const NO_RANK: Rank = Rank::NoRank;

    #[inline]
    pub fn from_u8(v: u8) -> Rank {
        match v {
            0 => Rank::Rank1,
            1 => Rank::Rank2,
            2 => Rank::Rank3,
            3 => Rank::Rank4,
            4 => Rank::Rank5,
            5 => Rank::Rank6,
            6 => Rank::Rank7,
            7 => Rank::Rank8,
            _ => Rank::NoRank,
        }
    }

    /// Parse from a character '1'–'8'.
    #[inline]
    pub fn from_char(c: char) -> Rank {
        if c >= '1' && c <= '8' {
            Rank::from_u8((c as u8) - b'1')
        } else {
            Rank::NoRank
        }
    }

    #[inline]
    pub fn as_u8(self) -> u8 {
        self as u8
    }

    #[inline]
    pub fn as_usize(self) -> usize {
        self as usize
    }

    /// Returns the bitboard mask for this rank.
    #[inline]
    pub fn bb(self) -> u64 {
        0xff_u64 << (8 * self.as_u8())
    }

    /// `true` if `r` is the back rank for `color`.
    /// White back rank = Rank1, Black back rank = Rank8.
    #[inline]
    pub fn back_rank(r: Rank, color: Color) -> bool {
        r == Rank::from_u8(color as u8 * 7)
    }

    /// Relative rank: rank XOR (color * 7).
    /// Used to flip ranks perspective for Black.
    #[inline]
    pub fn relative(r: Rank, color: Color) -> Rank {
        Rank::from_u8(r.as_u8() ^ (color as u8 * 7))
    }
}

impl std::fmt::Display for Rank {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if *self == Rank::NoRank {
            write!(f, "-")
        } else {
            write!(f, "{}", (b'1' + self.as_u8()) as char)
        }
    }
}

impl std::ops::Add<i32> for Rank {
    type Output = Rank;
    #[inline]
    fn add(self, rhs: i32) -> Rank {
        Rank::from_u8((self.as_u8() as i32 + rhs) as u8)
    }
}

impl std::ops::Sub<Rank> for Rank {
    type Output = i32;
    #[inline]
    fn sub(self, rhs: Rank) -> i32 {
        self.as_u8() as i32 - rhs.as_u8() as i32
    }
}

// ---------------------------------------------------------------------------
// Square
// ---------------------------------------------------------------------------

/// A chess square encoded as file + rank*8 (A1=0, B1=1, … H8=63), NO_SQ=64.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Square(pub u8);

impl Square {
    pub const NO_SQ: Square = Square(64);

    // Named constants for all 64 squares
    pub const SQ_A1: Square = Square(0);
    pub const SQ_B1: Square = Square(1);
    pub const SQ_C1: Square = Square(2);
    pub const SQ_D1: Square = Square(3);
    pub const SQ_E1: Square = Square(4);
    pub const SQ_F1: Square = Square(5);
    pub const SQ_G1: Square = Square(6);
    pub const SQ_H1: Square = Square(7);
    pub const SQ_A2: Square = Square(8);
    pub const SQ_B2: Square = Square(9);
    pub const SQ_C2: Square = Square(10);
    pub const SQ_D2: Square = Square(11);
    pub const SQ_E2: Square = Square(12);
    pub const SQ_F2: Square = Square(13);
    pub const SQ_G2: Square = Square(14);
    pub const SQ_H2: Square = Square(15);
    pub const SQ_A3: Square = Square(16);
    pub const SQ_B3: Square = Square(17);
    pub const SQ_C3: Square = Square(18);
    pub const SQ_D3: Square = Square(19);
    pub const SQ_E3: Square = Square(20);
    pub const SQ_F3: Square = Square(21);
    pub const SQ_G3: Square = Square(22);
    pub const SQ_H3: Square = Square(23);
    pub const SQ_A4: Square = Square(24);
    pub const SQ_B4: Square = Square(25);
    pub const SQ_C4: Square = Square(26);
    pub const SQ_D4: Square = Square(27);
    pub const SQ_E4: Square = Square(28);
    pub const SQ_F4: Square = Square(29);
    pub const SQ_G4: Square = Square(30);
    pub const SQ_H4: Square = Square(31);
    pub const SQ_A5: Square = Square(32);
    pub const SQ_B5: Square = Square(33);
    pub const SQ_C5: Square = Square(34);
    pub const SQ_D5: Square = Square(35);
    pub const SQ_E5: Square = Square(36);
    pub const SQ_F5: Square = Square(37);
    pub const SQ_G5: Square = Square(38);
    pub const SQ_H5: Square = Square(39);
    pub const SQ_A6: Square = Square(40);
    pub const SQ_B6: Square = Square(41);
    pub const SQ_C6: Square = Square(42);
    pub const SQ_D6: Square = Square(43);
    pub const SQ_E6: Square = Square(44);
    pub const SQ_F6: Square = Square(45);
    pub const SQ_G6: Square = Square(46);
    pub const SQ_H6: Square = Square(47);
    pub const SQ_A7: Square = Square(48);
    pub const SQ_B7: Square = Square(49);
    pub const SQ_C7: Square = Square(50);
    pub const SQ_D7: Square = Square(51);
    pub const SQ_E7: Square = Square(52);
    pub const SQ_F7: Square = Square(53);
    pub const SQ_G7: Square = Square(54);
    pub const SQ_H7: Square = Square(55);
    pub const SQ_A8: Square = Square(56);
    pub const SQ_B8: Square = Square(57);
    pub const SQ_C8: Square = Square(58);
    pub const SQ_D8: Square = Square(59);
    pub const SQ_E8: Square = Square(60);
    pub const SQ_F8: Square = Square(61);
    pub const SQ_G8: Square = Square(62);
    pub const SQ_H8: Square = Square(63);

    #[inline]
    pub fn new(file: File, rank: Rank) -> Square {
        Square(file.as_u8() + rank.as_u8() * 8)
    }

    /// Parse from algebraic notation like "e4".
    pub fn from_str(s: &str) -> Square {
        let bytes = s.as_bytes();
        debug_assert!(bytes.len() >= 2);
        let file = (bytes[0] - b'a') as u8;
        let rank = (bytes[1] - b'1') as u8;
        Square(file + rank * 8)
    }

    #[inline]
    pub fn index(self) -> usize {
        self.0 as usize
    }

    #[inline]
    pub fn is_valid(self) -> bool {
        self.0 < 64
    }

    #[inline]
    pub fn is_valid_rank_file(r: Rank, f: File) -> bool {
        r >= Rank::Rank1 && r <= Rank::Rank8 && f >= File::FileA && f <= File::FileH
    }

    #[inline]
    pub fn file(self) -> File {
        File::from_u8(self.0 & 7)
    }

    #[inline]
    pub fn rank(self) -> Rank {
        Rank::from_u8(self.0 >> 3)
    }

    #[inline]
    pub fn is_light(self) -> bool {
        (self.file().as_u8() + self.rank().as_u8()) & 1 != 0
    }

    #[inline]
    pub fn is_dark(self) -> bool {
        !self.is_light()
    }

    /// Chebyshev distance between two squares.
    pub fn distance(sq1: Square, sq2: Square) -> i32 {
        let df = (sq1.file().as_u8() as i32 - sq2.file().as_u8() as i32).abs();
        let dr = (sq1.rank().as_u8() as i32 - sq2.rank().as_u8() as i32).abs();
        df.max(dr)
    }

    /// Absolute difference of square indices.
    pub fn value_distance(sq1: Square, sq2: Square) -> i32 {
        (sq1.0 as i32 - sq2.0 as i32).abs()
    }

    /// `true` if both squares are the same light/dark color.
    #[inline]
    pub fn same_color(sq1: Square, sq2: Square) -> bool {
        ((9_u32.wrapping_mul((sq1.0 ^ sq2.0) as u32)) & 8) == 0
    }

    /// `true` if the square is on the back rank for the given color.
    #[inline]
    pub fn back_rank(sq: Square, color: Color) -> bool {
        Rank::back_rank(sq.rank(), color)
    }

    /// Flip the square vertically (XOR 56).
    #[inline]
    pub fn flip(self) -> Square {
        Square(self.0 ^ 56)
    }

    /// Relative square: XOR index with color*56.
    #[inline]
    pub fn relative_square(self, c: Color) -> Square {
        Square(self.0 ^ (c as u8 * 56))
    }

    #[inline]
    pub fn diagonal_of(self) -> i32 {
        7 + self.rank().as_u8() as i32 - self.file().as_u8() as i32
    }

    #[inline]
    pub fn antidiagonal_of(self) -> i32 {
        self.rank().as_u8() as i32 + self.file().as_u8() as i32
    }

    /// The en passant square: XOR with 8 (one rank up/down).
    /// Should only be called for valid ep positions.
    #[inline]
    pub fn ep_square(self) -> Square {
        Square(self.0 ^ 8)
    }

    /// King destination square after castling.
    #[inline]
    pub fn castling_king_square(is_king_side: bool, c: Color) -> Square {
        let base = if is_king_side {
            Square::SQ_G1
        } else {
            Square::SQ_C1
        };
        base.relative_square(c)
    }

    /// Rook destination square after castling.
    #[inline]
    pub fn castling_rook_square(is_king_side: bool, c: Color) -> Square {
        let base = if is_king_side {
            Square::SQ_F1
        } else {
            Square::SQ_D1
        };
        base.relative_square(c)
    }

    #[inline]
    pub fn max() -> usize {
        64
    }

    pub fn is_valid_string(s: &str) -> bool {
        let b = s.as_bytes();
        b.len() == 2 && b[0] >= b'a' && b[0] <= b'h' && b[1] >= b'1' && b[1] <= b'8'
    }
}

impl std::fmt::Display for Square {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if *self == Square::NO_SQ {
            write!(f, "-")
        } else {
            write!(f, "{}{}", self.file(), self.rank())
        }
    }
}

impl std::ops::BitXor for Square {
    type Output = Square;
    #[inline]
    fn bitxor(self, rhs: Square) -> Square {
        Square(self.0 ^ rhs.0)
    }
}

impl std::ops::Add<Direction> for Square {
    type Output = Square;
    #[inline]
    fn add(self, rhs: Direction) -> Square {
        Square((self.0 as i8 + rhs as i8) as u8)
    }
}

// ---------------------------------------------------------------------------
// Direction
// ---------------------------------------------------------------------------

// NOTE: Direction enum and its impl follow below.

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(i8)]
pub enum Direction {
    North = 8,
    West = -1,
    South = -8,
    East = 1,
    NorthEast = 9,
    NorthWest = 7,
    SouthWest = -9,
    SouthEast = -7,
}

impl Direction {
    /// Flip direction for Black: negate.
    #[inline]
    pub fn make_direction(dir: Direction, c: Color) -> Direction {
        if c == Color::Black {
            // SAFETY: negating the repr is valid for all direction values
            unsafe { std::mem::transmute(-(dir as i8)) }
        } else {
            dir
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::color::Color;

    // ---- File tests ----

    #[test]
    fn file_eq() {
        let f = File::FILE_A;
        assert_eq!(f, File::FILE_A);
    }

    #[test]
    fn file_ne() {
        let f = File::FILE_A;
        assert_ne!(f, File::FILE_B);
    }

    #[test]
    fn file_ge() {
        let f = File::FILE_A;
        assert!(f >= File::FILE_A);
    }

    #[test]
    fn file_le() {
        let f = File::FILE_A;
        assert!(f <= File::FILE_A);
    }

    #[test]
    fn file_lt() {
        let f = File::FILE_A;
        assert!(f < File::FILE_B);
    }

    #[test]
    fn file_gt() {
        let f = File::FILE_B;
        assert!(f > File::FILE_A);
    }

    #[test]
    fn file_as_int() {
        let f = File::FILE_A;
        assert_eq!(f.as_u8(), 0);
    }

    #[test]
    fn file_display() {
        assert_eq!(format!("{}", File::FILE_A), "a");
        assert_eq!(format!("{}", File::FILE_B), "b");
        assert_eq!(format!("{}", File::FILE_C), "c");
        assert_eq!(format!("{}", File::FILE_D), "d");
        assert_eq!(format!("{}", File::FILE_E), "e");
        assert_eq!(format!("{}", File::FILE_F), "f");
        assert_eq!(format!("{}", File::FILE_G), "g");
        assert_eq!(format!("{}", File::FILE_H), "h");
    }

    // ---- Rank tests ----

    #[test]
    fn rank_eq() {
        let r = Rank::RANK_1;
        assert_eq!(r, Rank::RANK_1);
    }

    #[test]
    fn rank_ne() {
        let r = Rank::RANK_1;
        assert_ne!(r, Rank::RANK_2);
    }

    #[test]
    fn rank_ge() {
        let r = Rank::RANK_1;
        assert!(r >= Rank::RANK_1);
    }

    #[test]
    fn rank_le() {
        let r = Rank::RANK_1;
        assert!(r <= Rank::RANK_1);
    }

    #[test]
    fn rank_as_int() {
        let r = Rank::RANK_1;
        assert_eq!(r.as_u8(), 0);
    }

    #[test]
    fn rank_display() {
        assert_eq!(format!("{}", Rank::RANK_1), "1");
        assert_eq!(format!("{}", Rank::RANK_2), "2");
        assert_eq!(format!("{}", Rank::RANK_3), "3");
        assert_eq!(format!("{}", Rank::RANK_4), "4");
        assert_eq!(format!("{}", Rank::RANK_5), "5");
        assert_eq!(format!("{}", Rank::RANK_6), "6");
        assert_eq!(format!("{}", Rank::RANK_7), "7");
        assert_eq!(format!("{}", Rank::RANK_8), "8");
    }

    // ---- Square tests ----

    #[test]
    fn square_eq() {
        let s = Square::SQ_A1;
        assert_eq!(s, Square::SQ_A1);
    }

    #[test]
    fn square_ne() {
        let s = Square::SQ_A1;
        assert_ne!(s, Square::SQ_A2);
    }

    #[test]
    fn square_ge() {
        let s = Square::SQ_A1;
        assert!(s >= Square::SQ_A1);
    }

    #[test]
    fn square_le() {
        let s = Square::SQ_A1;
        assert!(s <= Square::SQ_A1);
    }

    #[test]
    fn square_gt() {
        let s = Square::SQ_A2;
        assert!(s > Square::SQ_A1);
    }

    #[test]
    fn square_lt() {
        let s = Square::SQ_A1;
        assert!(s < Square::SQ_A2);
    }

    #[test]
    fn square_display() {
        assert_eq!(format!("{}", Square::SQ_A1), "a1");
        assert_eq!(format!("{}", Square::SQ_A2), "a2");
        assert_eq!(format!("{}", Square::SQ_A3), "a3");
        assert_eq!(format!("{}", Square::SQ_A4), "a4");
        assert_eq!(format!("{}", Square::SQ_A5), "a5");
        assert_eq!(format!("{}", Square::SQ_A6), "a6");
        assert_eq!(format!("{}", Square::SQ_A7), "a7");
        assert_eq!(format!("{}", Square::SQ_A8), "a8");
        assert_eq!(format!("{}", Square::SQ_B1), "b1");
        assert_eq!(format!("{}", Square::SQ_B2), "b2");
        assert_eq!(format!("{}", Square::SQ_B3), "b3");
        assert_eq!(format!("{}", Square::SQ_B4), "b4");
        assert_eq!(format!("{}", Square::SQ_B5), "b5");
        assert_eq!(format!("{}", Square::SQ_B6), "b6");
        assert_eq!(format!("{}", Square::SQ_B7), "b7");
        assert_eq!(format!("{}", Square::SQ_B8), "b8");
        assert_eq!(format!("{}", Square::SQ_C1), "c1");
        assert_eq!(format!("{}", Square::SQ_C2), "c2");
        assert_eq!(format!("{}", Square::SQ_C3), "c3");
    }

    #[test]
    fn square_file() {
        assert_eq!(Square::SQ_A1.file(), File::FILE_A);
        assert_eq!(Square::SQ_B1.file(), File::FILE_B);
        assert_eq!(Square::SQ_C1.file(), File::FILE_C);
    }

    #[test]
    fn square_rank() {
        assert_eq!(Square::SQ_A1.rank(), Rank::RANK_1);
        assert_eq!(Square::SQ_A2.rank(), Rank::RANK_2);
        assert_eq!(Square::SQ_A3.rank(), Rank::RANK_3);
    }

    #[test]
    fn square_new_from_file_rank() {
        assert_eq!(Square::new(File::FILE_A, Rank::RANK_1), Square::SQ_A1);
        assert_eq!(Square::new(File::FILE_B, Rank::RANK_1), Square::SQ_B1);
        assert_eq!(Square::new(File::FILE_C, Rank::RANK_1), Square::SQ_C1);
    }

    #[test]
    fn square_from_str() {
        assert_eq!(Square::from_str("a1"), Square::SQ_A1);
        assert_eq!(Square::from_str("b1"), Square::SQ_B1);
        assert_eq!(Square::from_str("c1"), Square::SQ_C1);
    }

    #[test]
    fn square_is_light() {
        assert!(!Square::SQ_A1.is_light());
        assert!(Square::SQ_B1.is_light());
        assert!(!Square::SQ_C1.is_light());
    }

    #[test]
    fn square_is_dark() {
        assert!(Square::SQ_A1.is_dark());
        assert!(!Square::SQ_B1.is_dark());
        assert!(Square::SQ_C1.is_dark());
    }

    #[test]
    fn square_is_valid() {
        assert!(Square::SQ_A1.is_valid());
        assert!(Square::SQ_B1.is_valid());
        assert!(Square::SQ_C1.is_valid());
        assert!(!Square::NO_SQ.is_valid());
    }

    #[test]
    fn square_is_valid_rank_file() {
        assert!(Square::is_valid_rank_file(Rank::RANK_1, File::FILE_A));
        assert!(Square::is_valid_rank_file(Rank::RANK_1, File::FILE_B));
        assert!(!Square::is_valid_rank_file(Rank::RANK_1, File::NO_FILE));
    }

    #[test]
    fn square_distance() {
        assert_eq!(Square::distance(Square::SQ_A1, Square::SQ_A1), 0);
        assert_eq!(Square::distance(Square::SQ_A1, Square::SQ_A2), 1);
        assert_eq!(Square::distance(Square::SQ_A1, Square::SQ_B1), 1);
        assert_eq!(Square::distance(Square::SQ_A1, Square::SQ_B2), 1);
    }

    #[test]
    fn square_value_distance() {
        assert_eq!(Square::value_distance(Square::SQ_A1, Square::SQ_A1), 0);
        assert_eq!(Square::value_distance(Square::SQ_A1, Square::SQ_A2), 8);
        assert_eq!(Square::value_distance(Square::SQ_A1, Square::SQ_B1), 1);
        assert_eq!(Square::value_distance(Square::SQ_A1, Square::SQ_B2), 9);
    }

    #[test]
    fn square_same_color() {
        assert!(Square::same_color(Square::SQ_A1, Square::SQ_A1));
        assert!(!Square::same_color(Square::SQ_A1, Square::SQ_A2));
        assert!(!Square::same_color(Square::SQ_A1, Square::SQ_B1));
        assert!(Square::same_color(Square::SQ_A1, Square::SQ_B2));
    }

    #[test]
    fn square_back_rank() {
        assert!(Square::back_rank(Square::SQ_A1, Color::White));
        assert!(!Square::back_rank(Square::SQ_A1, Color::Black));
        assert!(!Square::back_rank(Square::SQ_A8, Color::White));
        assert!(Square::back_rank(Square::SQ_A8, Color::Black));
    }

    #[test]
    fn square_flip() {
        assert_eq!(Square::SQ_A1.flip(), Square::SQ_A8);
        assert_eq!(Square::SQ_A2.flip(), Square::SQ_A7);
        assert_eq!(Square::SQ_A3.flip(), Square::SQ_A6);
    }

    #[test]
    fn square_relative_square() {
        assert_eq!(Square::SQ_A1.relative_square(Color::White), Square::SQ_A1);
        assert_eq!(Square::SQ_A1.relative_square(Color::Black), Square::SQ_A8);
        assert_eq!(Square::SQ_A2.relative_square(Color::White), Square::SQ_A2);
        assert_eq!(Square::SQ_A2.relative_square(Color::Black), Square::SQ_A7);
    }

    #[test]
    fn square_ep_square() {
        assert_eq!(Square::SQ_A3.ep_square(), Square::SQ_A4);
        assert_eq!(Square::SQ_A4.ep_square(), Square::SQ_A3);
        assert_eq!(Square::SQ_A5.ep_square(), Square::SQ_A6);
        assert_eq!(Square::SQ_A6.ep_square(), Square::SQ_A5);
    }

    #[test]
    fn square_castling_king_square() {
        assert_eq!(
            Square::castling_king_square(true, Color::White),
            Square::SQ_G1
        );
        assert_eq!(
            Square::castling_king_square(false, Color::White),
            Square::SQ_C1
        );
        assert_eq!(
            Square::castling_king_square(true, Color::Black),
            Square::SQ_G8
        );
        assert_eq!(
            Square::castling_king_square(false, Color::Black),
            Square::SQ_C8
        );
    }

    #[test]
    fn square_max() {
        assert_eq!(Square::max(), 64);
    }
}
