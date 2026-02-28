use crate::variants::chessport::coords::{File, Square};
use crate::variants::chessport::piece::Piece;

/// Zobrist hashing tables — a direct port of `src/zobrist.hpp`.
pub struct Zobrist;

impl Zobrist {
    const fn make_array() -> [u64; 781] {
        let mut arr = [0u64; 781];
        let mut state: u64 = 0xdeadbeefcafebabe;
        let mut i = 0;
        while i < 781 {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            arr[i] = state.wrapping_mul(0x2545F4914F6CDD1D);
            i += 1;
        }
        arr
    }

    const RANDOM_ARRAY: [u64; 781] = Self::make_array();

    /// index mapping: MAP_HASH_PIECE[piece_index]
    const MAP_HASH_PIECE: [usize; 12] = [1, 3, 5, 7, 9, 11, 0, 2, 4, 6, 8, 10];

    /// Castling key table: index 0–15 (4-bit castling rights bitmask).
    const CASTLING_KEY: [u64; 16] = {
        let mut arr = [0u64; 16];
        let mut i = 0usize;
        while i < 16 {
            let mut key = 0u64;
            let mut b = 0usize;
            while b < 4 {
                if i & (1 << b) != 0 {
                    key ^= Self::RANDOM_ARRAY[768 + b];
                }
                b += 1;
            }
            arr[i] = key;
            i += 1;
        }
        arr
    };

    /// Hash for piece on square.
    #[inline(always)]
    pub fn piece(piece: Piece, sq: Square) -> u64 {
        debug_assert!((usize::from(piece)) < 12);
        Self::RANDOM_ARRAY[64 * Self::MAP_HASH_PIECE[usize::from(piece)] + sq.0 as usize]
    }

    /// Hash for en-passant file.
    #[inline(always)]
    pub fn enpassant(file: File) -> u64 {
        debug_assert!((file as usize) < 8);
        Self::RANDOM_ARRAY[772 + file as usize]
    }

    /// Hash for full castling rights bitmask (0–15).
    #[inline(always)]
    pub fn castling(castling: usize) -> u64 {
        debug_assert!(castling < 16);
        Self::CASTLING_KEY[castling]
    }

    /// Hash for a single castling-right index (0–3).
    #[inline(always)]
    pub fn castling_index(idx: usize) -> u64 {
        debug_assert!(idx < 4);
        Self::RANDOM_ARRAY[768 + idx]
    }

    /// Hash mixed in when it's White's turn to move.
    #[inline(always)]
    pub fn side_to_move() -> u64 {
        Self::RANDOM_ARRAY[780]
    }
}
