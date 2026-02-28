/// Represents the color of a chess piece or side to move.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(i8)]
pub enum Color {
    White = 0,
    Black = 1,
    None = -1,
}

impl Color {
    pub const WHITE: Color = Color::White;
    pub const BLACK: Color = Color::Black;
    pub const NONE: Color = Color::None;

    /// Returns the long string representation: "White", "Black", or "None".
    pub fn long_str(self) -> &'static str {
        match self {
            Color::White => "White",
            Color::Black => "Black",
            Color::None => "None",
        }
    }

    /// Toggle the color. Panics if called on Color::None.
    #[inline]
    pub fn flip(self) -> Color {
        match self {
            Color::White => Color::Black,
            Color::Black => Color::White,
            Color::None => panic!("Cannot flip Color::None"),
        }
    }

    /// Returns `true` if this is a valid playing color (White or Black).
    #[inline]
    pub fn is_valid(self) -> bool {
        !matches!(self, Color::None)
    }

    /// Convert to integer: White=0, Black=1. Panics for None.
    #[inline]
    pub fn index(self) -> usize {
        match self {
            Color::White => 0,
            Color::Black => 1,
            Color::None => panic!("Color::None has no index"),
        }
    }

    /// Convert from integer 0/1 to White/Black, -1 to None.
    #[inline]
    pub fn from_i8(v: i8) -> Color {
        match v {
            0 => Color::White,
            1 => Color::Black,
            -1 => Color::None,
            _ => panic!("Invalid color value: {}", v),
        }
    }
}

impl std::ops::Not for Color {
    type Output = Color;
    #[inline]
    fn not(self) -> Color {
        self.flip()
    }
}

impl std::fmt::Display for Color {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Color::White => write!(f, "w"),
            Color::Black => write!(f, "b"),
            Color::None => write!(f, "NONE"),
        }
    }
}

impl TryFrom<&str> for Color {
    type Error = ();
    fn try_from(s: &str) -> Result<Self, ()> {
        match s {
            "w" => Ok(Color::White),
            "b" => Ok(Color::Black),
            _ => Err(()),
        }
    }
}

impl From<Color> for i8 {
    #[inline]
    fn from(c: Color) -> i8 {
        c as i8
    }
}

impl From<Color> for usize {
    #[inline]
    fn from(c: Color) -> usize {
        match c {
            Color::White => 0,
            Color::Black => 1,
            Color::None => panic!("Color::None cannot be used as usize"),
        }
    }
}

// ---------------------------------------------------------------------------
// Tests (ported from tests/color.cpp)
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn color_not_operator() {
        let c = Color::WHITE;
        assert_eq!(!c, Color::BLACK);
        let c = Color::BLACK;
        assert_eq!(!c, Color::WHITE);
    }

    #[test]
    fn color_eq() {
        let c = Color::WHITE;
        assert_eq!(c, Color::WHITE);
        let c = Color::BLACK;
        assert_eq!(c, Color::BLACK);
    }

    #[test]
    fn color_ne() {
        let c = Color::WHITE;
        assert_ne!(c, Color::BLACK);
        let c = Color::BLACK;
        assert_ne!(c, Color::WHITE);
    }

    #[test]
    fn color_as_int() {
        let c = Color::WHITE;
        assert_eq!(c as i8, 0);
        let c = Color::BLACK;
        assert_eq!(c as i8, 1);
    }

    #[test]
    fn color_display() {
        let c = Color::WHITE;
        assert_eq!(format!("{}", c), "w");
        let c = Color::BLACK;
        assert_eq!(format!("{}", c), "b");
    }

    #[test]
    fn color_internal() {
        let c = Color::WHITE;
        assert_eq!(c, Color::WHITE);
        let c = Color::BLACK;
        assert_eq!(c, Color::BLACK);
    }

    #[test]
    fn color_from_string() {
        let c = Color::try_from("w").unwrap();
        assert_eq!(c, Color::WHITE);
        let c = Color::try_from("b").unwrap();
        assert_eq!(c, Color::BLACK);
    }
}
