use crate::variants::chessport::coords::Square;
use crate::variants::chessport::piece::PieceType;

/// 16-bit move encoding.
///
/// Bits  0- 5: target square (to)
/// Bits  6-11: source square (from)
/// Bits 12-13: promotion piece offset (0=Knight, 1=Bishop, 2=Rook, 3=Queen)
/// Bits 14-15: move type (0=Normal, 1=Promotion, 2=EnPassant, 3=Castling)
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct Move {
    move_: u16,
    score_: i16,
}

impl Move {
    pub const NO_MOVE: u16 = 0;
    pub const NULL_MOVE: u16 = 65;
    pub const NORMAL: u16 = 0;
    pub const PROMOTION: u16 = 1 << 14;
    pub const ENPASSANT: u16 = 2 << 14;
    pub const CASTLING: u16 = 3 << 14;

    #[inline(always)]
    pub const fn from_raw(raw: u16) -> Self {
        Move {
            move_: raw,
            score_: 0,
        }
    }

    /// Construct a move. `MOVE_TYPE` should be one of the constants above.
    /// `pt` is only relevant for promotions; pass `PieceType::KNIGHT` otherwise.
    #[inline(always)]
    pub fn make<const MOVE_TYPE: u16>(source: Square, target: Square, pt: PieceType) -> Self {
        // promotion piece offset: Knight=0, Bishop=1, Rook=2, Queen=3
        let promo_offset = (pt.as_u8() as u16).saturating_sub(PieceType::KNIGHT.as_u8() as u16);
        let raw = MOVE_TYPE | (promo_offset << 12) | ((source.0 as u16) << 6) | (target.0 as u16);
        Move {
            move_: raw,
            score_: 0,
        }
    }

    /// Source square.
    #[inline(always)]
    pub fn from(self) -> Square {
        Square(((self.move_ >> 6) & 0x3F) as u8)
    }

    /// Target square.
    #[inline(always)]
    pub fn to(self) -> Square {
        Square((self.move_ & 0x3F) as u8)
    }

    /// Move type: one of `NORMAL`, `PROMOTION`, `ENPASSANT`, `CASTLING`.
    #[inline(always)]
    pub fn type_of(self) -> u16 {
        self.move_ & (3 << 14)
    }

    /// The promotion piece type (only valid when `type_of() == PROMOTION`).
    #[inline(always)]
    pub fn promotion_type(self) -> PieceType {
        let offset = ((self.move_ >> 12) & 3) as u8;
        // offset 0=Knight, 1=Bishop, 2=Rook, 3=Queen
        match offset {
            0 => PieceType::KNIGHT,
            1 => PieceType::BISHOP,
            2 => PieceType::ROOK,
            3 => PieceType::QUEEN,
            _ => PieceType::NONE,
        }
    }

    /// Set the sorting score.
    #[inline(always)]
    pub fn set_score(&mut self, score: i16) {
        self.score_ = score;
    }

    #[inline(always)]
    pub fn raw(self) -> u16 {
        self.move_
    }

    #[inline(always)]
    pub fn score(self) -> i16 {
        self.score_
    }

    #[inline(always)]
    pub fn is_null(self) -> bool {
        self.move_ == Self::NO_MOVE
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::coords::Square;
    use crate::variants::chessport::piece::PieceType;

    #[test]
    fn move_from() {
        assert_eq!(
            Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A2, PieceType::KNIGHT).from(),
            Square::SQ_A1
        );
        assert_eq!(
            Move::make::<{ Move::NORMAL }>(Square::SQ_H8, Square::SQ_H1, PieceType::KNIGHT).from(),
            Square::SQ_H8
        );
    }

    #[test]
    fn move_to() {
        assert_eq!(
            Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A2, PieceType::KNIGHT).to(),
            Square::SQ_A2
        );
        assert_eq!(
            Move::make::<{ Move::NORMAL }>(Square::SQ_H8, Square::SQ_H1, PieceType::KNIGHT).to(),
            Square::SQ_H1
        );
    }

    #[test]
    fn move_type_of() {
        assert_eq!(
            Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A2, PieceType::KNIGHT)
                .type_of(),
            Move::NORMAL
        );
        assert_eq!(
            Move::make::<{ Move::PROMOTION }>(Square::SQ_H7, Square::SQ_H8, PieceType::KNIGHT)
                .type_of(),
            Move::PROMOTION
        );
        assert_eq!(
            Move::make::<{ Move::ENPASSANT }>(Square::SQ_D5, Square::SQ_C6, PieceType::KNIGHT)
                .type_of(),
            Move::ENPASSANT
        );
        assert_eq!(
            Move::make::<{ Move::CASTLING }>(Square::SQ_E8, Square::SQ_H8, PieceType::KNIGHT)
                .type_of(),
            Move::CASTLING
        );
    }

    #[test]
    fn move_promotion_type() {
        assert_eq!(
            Move::make::<{ Move::PROMOTION }>(Square::SQ_A1, Square::SQ_A2, PieceType::BISHOP)
                .promotion_type(),
            PieceType::BISHOP
        );
        assert_eq!(
            Move::make::<{ Move::PROMOTION }>(Square::SQ_H7, Square::SQ_H8, PieceType::KNIGHT)
                .promotion_type(),
            PieceType::KNIGHT
        );
        assert_eq!(
            Move::make::<{ Move::PROMOTION }>(Square::SQ_D5, Square::SQ_C6, PieceType::ROOK)
                .promotion_type(),
            PieceType::ROOK
        );
        assert_eq!(
            Move::make::<{ Move::PROMOTION }>(Square::SQ_E8, Square::SQ_H8, PieceType::QUEEN)
                .promotion_type(),
            PieceType::QUEEN
        );
    }
}
