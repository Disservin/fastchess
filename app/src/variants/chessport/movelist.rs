use crate::variants::chessport::chess_move::Move;
use crate::variants::chessport::constants::MAX_MOVES;

pub struct Movelist {
    moves: [Move; MAX_MOVES],
    size: usize,
}

impl Movelist {
    pub fn new() -> Self {
        Movelist {
            moves: [Move::default(); MAX_MOVES],
            size: 0,
        }
    }

    // Element access

    pub fn at(&self, pos: usize) -> &Move {
        assert!(pos < self.size, "Movelist index out of bounds");
        &self.moves[pos]
    }

    pub fn at_mut(&mut self, pos: usize) -> &mut Move {
        assert!(pos < self.size, "Movelist index out of bounds");
        &mut self.moves[pos]
    }

    pub fn front(&self) -> &Move {
        &self.moves[0]
    }

    pub fn back(&self) -> &Move {
        &self.moves[self.size - 1]
    }

    // Capacity

    pub fn is_empty(&self) -> bool {
        self.size == 0
    }

    pub fn size(&self) -> usize {
        self.size
    }

    // Modifiers

    pub fn clear(&mut self) {
        self.size = 0;
    }

    pub fn add(&mut self, mv: Move) {
        debug_assert!(self.size < MAX_MOVES);
        self.moves[self.size] = mv;
        self.size += 1;
    }

    // Iterators

    pub fn iter(&self) -> std::slice::Iter<'_, Move> {
        self.moves[..self.size].iter()
    }

    pub fn iter_mut(&mut self) -> std::slice::IterMut<'_, Move> {
        self.moves[..self.size].iter_mut()
    }
}

impl Default for Movelist {
    fn default() -> Self {
        Self::new()
    }
}

impl std::ops::Index<usize> for Movelist {
    type Output = Move;
    fn index(&self, idx: usize) -> &Move {
        &self.moves[idx]
    }
}

impl std::ops::IndexMut<usize> for Movelist {
    fn index_mut(&mut self, idx: usize) -> &mut Move {
        &mut self.moves[idx]
    }
}

impl<'a> IntoIterator for &'a Movelist {
    type Item = &'a Move;
    type IntoIter = std::slice::Iter<'a, Move>;
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::chess_move::Move;
    use crate::variants::chessport::coords::Square;
    use crate::variants::chessport::piece::PieceType;

    #[test]
    fn movelist_add() {
        let mut moves = Movelist::new();
        moves.add(Move::make::<{ Move::NORMAL }>(
            Square::SQ_A1,
            Square::SQ_A2,
            PieceType::KNIGHT,
        ));

        assert_eq!(moves.size(), 1);
        assert_eq!(
            moves[0],
            Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A2, PieceType::KNIGHT)
        );
        assert!(!moves.is_empty());
    }

    #[test]
    fn movelist_find() {
        let mut moves = Movelist::new();
        moves.add(Move::make::<{ Move::NORMAL }>(
            Square::SQ_A1,
            Square::SQ_A2,
            PieceType::KNIGHT,
        ));
        moves.add(Move::make::<{ Move::NORMAL }>(
            Square::SQ_A1,
            Square::SQ_A3,
            PieceType::KNIGHT,
        ));
        moves.add(Move::make::<{ Move::NORMAL }>(
            Square::SQ_A1,
            Square::SQ_A4,
            PieceType::KNIGHT,
        ));

        assert!(moves.iter().any(|&m| m
            == Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A2, PieceType::KNIGHT)));
        assert!(moves.iter().any(|&m| m
            == Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A3, PieceType::KNIGHT)));
        assert!(moves.iter().any(|&m| m
            == Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A4, PieceType::KNIGHT)));
        assert!(!moves.iter().any(|&m| m
            == Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A5, PieceType::KNIGHT)));
    }
}
