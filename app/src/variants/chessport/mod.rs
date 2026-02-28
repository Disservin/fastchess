// Declare the submodules
pub mod attacks;
pub mod bitboard;
pub mod board;
pub mod chess_move;
pub mod color;
pub mod constants;
pub mod coords;
pub mod movegen;
pub mod movelist;
pub mod pgn;
pub mod piece;
pub mod uci;
pub mod utils;
pub mod zobrist;

// Re-export all public items from submodules to the 'chessport' level
pub use attacks::*;
pub use bitboard::*;
pub use board::*;
pub use chess_move::*;
pub use color::*;
pub use constants::*;
pub use coords::*;
pub use movegen::*;
pub use movelist::*;
pub use pgn::*;
pub use piece::*;
pub use uci::*;
pub use utils::*;
pub use zobrist::*;
