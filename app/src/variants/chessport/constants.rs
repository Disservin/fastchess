use crate::variants::chessport::bitboard::Bitboard;

pub const DEFAULT_CHECKMASK: Bitboard = Bitboard(0xFFFF_FFFF_FFFF_FFFFu64);
pub const STARTPOS: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
pub const MAX_MOVES: usize = 256;
