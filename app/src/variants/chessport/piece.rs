use std::convert::TryFrom;
use std::fmt;

use crate::variants::chessport::color::Color;

/// The type of a piece (without color).
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub struct PieceType(u8);

impl PieceType {
    pub const PAWN: Self = Self(0);
    pub const KNIGHT: Self = Self(1);
    pub const BISHOP: Self = Self(2);
    pub const ROOK: Self = Self(3);
    pub const QUEEN: Self = Self(4);
    pub const KING: Self = Self(5);
    pub const NONE: Self = Self(6);

    pub const ALL: [Self; 6] = [
        Self::PAWN,
        Self::KNIGHT,
        Self::BISHOP,
        Self::ROOK,
        Self::QUEEN,
        Self::KING,
    ];

    pub fn from_char(c: char) -> Self {
        match c.to_ascii_lowercase() {
            'p' => Self::PAWN,
            'n' => Self::KNIGHT,
            'b' => Self::BISHOP,
            'r' => Self::ROOK,
            'q' => Self::QUEEN,
            'k' => Self::KING,
            _ => Self::NONE,
        }
    }

    pub fn as_char(self) -> char {
        if self == Self::NONE {
            return ' ';
        }
        b"pnbrqk"[self.0 as usize] as char
    }

    #[inline(always)]
    pub fn as_u8(self) -> u8 {
        self.0
    }
}

impl fmt::Display for PieceType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_char())
    }
}

/// A chess piece (color + type packed as a single u8, 0-11, 12 = None).
///
/// Encoding:  `color * 6 + type`
///   0 = WhitePawn, 1 = WhiteKnight, ..., 5 = WhiteKing
///   6 = BlackPawn, 7 = BlackKnight, ..., 11 = BlackKing
///   12 = None
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub struct Piece(u8);

impl Piece {
    pub const WHITE_PAWN: Self = Self(0);
    pub const WHITE_KNIGHT: Self = Self(1);
    pub const WHITE_BISHOP: Self = Self(2);
    pub const WHITE_ROOK: Self = Self(3);
    pub const WHITE_QUEEN: Self = Self(4);
    pub const WHITE_KING: Self = Self(5);
    pub const BLACK_PAWN: Self = Self(6);
    pub const BLACK_KNIGHT: Self = Self(7);
    pub const BLACK_BISHOP: Self = Self(8);
    pub const BLACK_ROOK: Self = Self(9);
    pub const BLACK_QUEEN: Self = Self(10);
    pub const BLACK_KING: Self = Self(11);
    pub const NONE: Self = Self(12);

    #[inline(always)]
    pub fn new(pt: PieceType, color: Color) -> Self {
        if pt == PieceType::NONE || color == Color::NONE {
            return Self::NONE;
        }
        Self(color as u8 * 6 + pt.as_u8())
    }

    #[inline(always)]
    pub fn from_u8(v: u8) -> Self {
        if v <= 12 {
            Self(v)
        } else {
            Self::NONE
        }
    }

    #[inline(always)]
    pub fn as_u8(self) -> u8 {
        self.0
    }

    pub fn from_char(c: char) -> Self {
        let pt = PieceType::from_char(c);
        Self::new(
            pt,
            if c.is_uppercase() {
                Color::WHITE
            } else {
                Color::BLACK
            },
        )
    }

    pub fn piece_type(self) -> PieceType {
        if self == Self::NONE {
            PieceType::NONE
        } else {
            PieceType::ALL[(self.0 % 6) as usize]
        }
    }

    pub fn color(self) -> Color {
        match self.0 {
            0..=5 => Color::WHITE,
            6..=11 => Color::BLACK,
            _ => Color::NONE,
        }
    }

    pub fn as_char(self) -> char {
        if self == Self::NONE {
            return '.';
        }
        let c = self.piece_type().as_char();
        if self.color() == Color::WHITE {
            c.to_ascii_uppercase()
        } else {
            c
        }
    }
}

impl TryFrom<u8> for Piece {
    type Error = ();
    fn try_from(v: u8) -> Result<Self, Self::Error> {
        if v <= 12 {
            Ok(Self(v))
        } else {
            Err(())
        }
    }
}

impl From<Piece> for usize {
    #[inline(always)]
    fn from(p: Piece) -> usize {
        p.0 as usize
    }
}

impl fmt::Display for Piece {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_char())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::color::Color;

    #[test]
    fn test_piece_type_basics() {
        // Test a sample of types for display and char conversion
        assert_eq!(PieceType::PAWN.as_char(), 'p');
        assert_eq!(PieceType::from_char('N'), PieceType::KNIGHT);
        assert_eq!(PieceType::NONE.as_char(), ' ');
    }

    #[test]
    fn test_piece_identity() {
        let white_rook = Piece::new(PieceType::ROOK, Color::White);
        let black_king = Piece::new(PieceType::KING, Color::Black);

        // Verify type and color extraction
        assert_eq!(white_rook.piece_type(), PieceType::ROOK);
        assert_eq!(white_rook.color(), Color::White);
        assert_eq!(black_king.piece_type(), PieceType::KING);
        assert_eq!(black_king.color(), Color::Black);
    }

    #[test]
    fn test_piece_serialization() {
        // Table-driven test: check one of each "class" of piece
        let cases = [
            (Piece::WHITE_PAWN, "P", 0),
            (Piece::BLACK_QUEEN, "q", 10),
            (Piece::NONE, ".", 12),
        ];

        for (piece, expected_char, expected_val) in cases {
            assert_eq!(format!("{piece}"), expected_char);
            assert_eq!(usize::from(piece), expected_val);
            assert_eq!(
                Piece::from_char(expected_char.chars().next().unwrap()),
                piece
            );
        }
    }

    #[test]
    fn test_none_behavior() {
        // Ensure "None" inputs always result in Piece::None
        assert_eq!(Piece::new(PieceType::NONE, Color::White), Piece::NONE);
        assert_eq!(Piece::new(PieceType::PAWN, Color::None), Piece::NONE);
        assert_eq!(Piece::NONE.color(), Color::None);
        assert_eq!(Piece::NONE.piece_type(), PieceType::NONE);
    }
}
