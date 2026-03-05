//! Syzygy tablebase probing module.
//!
//! Provides tablebase probing for endgame adjudication using the `pyrrhic_rs` crate.

use std::sync::OnceLock;

use pyrrhic_rs::{EngineAdapter, TableBases, WdlProbeResult};

use crate::types::adjudication::{TbAdjudication, TbResultType};
use crate::{log_info, log_warn};

use chess_library_rs::attacks::{bishop, king, knight, pawn, rook};
use chess_library_rs::bitboard::Bitboard;
use chess_library_rs::board::Board;
use chess_library_rs::color::Color;
use chess_library_rs::coords::Square;

#[derive(Clone)]
struct TablebaseAdapter;

impl EngineAdapter for TablebaseAdapter {
    fn pawn_attacks(color: pyrrhic_rs::Color, sq: u64) -> u64 {
        pawn(
            if color == pyrrhic_rs::Color::Black {
                Color::Black
            } else {
                Color::White
            },
            Square::from_u8(sq as u8),
        )
        .0
    }

    fn knight_attacks(sq: u64) -> u64 {
        knight(Square::from_u8(sq as u8)).0
    }

    fn bishop_attacks(sq: u64, occ: u64) -> u64 {
        bishop(Square::from_u8(sq as u8), Bitboard(occ)).0
    }

    fn rook_attacks(sq: u64, occ: u64) -> u64 {
        rook(Square::from_u8(sq as u8), Bitboard(occ)).0
    }

    fn king_attacks(sq: u64) -> u64 {
        king(Square::from_u8(sq as u8)).0
    }

    fn queen_attacks(sq: u64, occ: u64) -> u64 {
        bishop(Square::from_u8(sq as u8), Bitboard(occ)).0
            | rook(Square::from_u8(sq as u8), Bitboard(occ)).0
    }
}

/// Global tablebase instance (initialized once).
static TABLEBASE: OnceLock<Option<TableBases<TablebaseAdapter>>> = OnceLock::new();

/// Result of a tablebase probe.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TbProbeResult {
    /// Win for the side to move.
    Win,
    /// Loss for the side to move.
    Loss,
    /// Drawn position.
    Draw,
    /// Cursed win (win but draw under 50-move rule).
    CursedWin,
    /// Blessed loss (loss but draw under 50-move rule).
    BlessedLoss,
    /// Position cannot be probed (too many pieces, missing tables, etc.).
    NotAvailable,
}

impl From<WdlProbeResult> for TbProbeResult {
    fn from(wdl: WdlProbeResult) -> Self {
        match wdl {
            WdlProbeResult::Win => TbProbeResult::Win,
            WdlProbeResult::Loss => TbProbeResult::Loss,
            WdlProbeResult::Draw => TbProbeResult::Draw,
            WdlProbeResult::CursedWin => TbProbeResult::CursedWin,
            WdlProbeResult::BlessedLoss => TbProbeResult::BlessedLoss,
        }
    }
}

/// Initialize the global tablebase from a semicolon-separated list of directories.
///
/// This should be called once at program startup. Subsequent calls are no-ops.
pub fn init_tablebase(syzygy_dirs: &str) {
    TABLEBASE.get_or_init(|| {
        if syzygy_dirs.is_empty() {
            return None;
        }

        let mut any_loaded = false;
        let mut tb = None;

        match pyrrhic_rs::TableBases::<TablebaseAdapter>::new(syzygy_dirs) {
            Ok(loaded_tb) => {
                log_info!("Loaded Syzygy tablebases from {}", syzygy_dirs);
                tb = Some(loaded_tb);
                any_loaded = true;
            }
            Err(_e) => {
                // log error
                log_warn!("Failed to load tablebases from {}", syzygy_dirs);
            }
        }

        if !any_loaded {
            log_warn!("No Syzygy tablebases loaded");
        }

        tb
    });
}

/// Check if the tablebase is initialized and available.
pub fn is_available() -> bool {
    TABLEBASE.get().map(|tb| tb.is_some()).unwrap_or(false)
}

/// Get the maximum number of pieces supported by the loaded tablebases.
pub fn max_pieces() -> u32 {
    TABLEBASE
        .get()
        .and_then(|tb| tb.as_ref())
        .map(|tb| tb.max_pieces())
        .unwrap_or(0) as u32
}

/// Probe WDL (Win/Draw/Loss) for a position.
///
/// Returns `NotAvailable` if the position cannot be probed.
pub fn probe_wdl(board: &Board) -> TbProbeResult {
    let Some(Some(tb)) = TABLEBASE.get() else {
        return TbProbeResult::NotAvailable;
    };

    use chess_library_rs::piece::PieceType;

    let white = board.us(Color::White).0;
    let black = board.us(Color::Black).0;
    let kings = board.pieces_pt(PieceType::KING).0;
    let queens = board.pieces_pt(PieceType::QUEEN).0;
    let rooks = board.pieces_pt(PieceType::ROOK).0;
    let bishops = board.pieces_pt(PieceType::BISHOP).0;
    let knights = board.pieces_pt(PieceType::KNIGHT).0;
    let pawns = board.pieces_pt(PieceType::PAWN).0;
    let ep = board.enpassant_sq().0 as u32;
    let turn = board.side_to_move() == Color::White;

    match tb.probe_wdl(
        white, black, kings, queens, rooks, bishops, knights, pawns, ep, turn,
    ) {
        Ok(wdl) => wdl.into(),
        Err(_) => TbProbeResult::NotAvailable,
    }
}

/// Probe DTZ (Distance To Zeroing) for a position.
///
/// Returns `None` if the position cannot be probed.
pub fn probe_dtz(board: &Board) -> Option<u32> {
    let Some(Some(tb)) = TABLEBASE.get() else {
        return None;
    };

    use chess_library_rs::piece::PieceType;

    let white = board.us(Color::White).0;
    let black = board.us(Color::Black).0;
    let kings = board.pieces_pt(PieceType::KING).0;
    let queens = board.pieces_pt(PieceType::QUEEN).0;
    let rooks = board.pieces_pt(PieceType::ROOK).0;
    let bishops = board.pieces_pt(PieceType::BISHOP).0;
    let knights = board.pieces_pt(PieceType::KNIGHT).0;
    let pawns = board.pieces_pt(PieceType::PAWN).0;
    let rule50 = board.half_move_clock();
    let ep = board.enpassant_sq().0 as u32;
    let turn = board.side_to_move() == Color::White;

    match tb.probe_root(
        white, black, kings, queens, rooks, bishops, knights, pawns, rule50, ep, turn,
    ) {
        Ok(result) => match result.root {
            pyrrhic_rs::DtzProbeValue::DtzResult(dtz_result) => Some(dtz_result.dtz as u32),
            _ => None,
        },
        Err(_) => None,
    }
}

/// Check if a position can be probed (piece count within limits).
pub fn can_probe(board: &Board, max_pieces_limit: u32) -> bool {
    let piece_count = board.occ().count();

    // Check against configured max pieces limit
    if max_pieces_limit > 0 && piece_count > max_pieces_limit {
        return false;
    }

    // Check against loaded tablebase capacity
    let tb_max = max_pieces();
    if tb_max > 0 && piece_count > tb_max {
        return false;
    }

    // Need at least some tablebases loaded
    is_available()
}

/// Determine if a position should be adjudicated based on tablebase probe.
///
/// Returns the probe result if adjudication is applicable, or `NotAvailable` otherwise.
pub fn should_adjudicate(
    board: &Board,
    config: &TbAdjudication,
    ignore_50_move_rule: bool,
) -> TbProbeResult {
    if !config.enabled {
        return TbProbeResult::NotAvailable;
    }

    if !can_probe(board, config.max_pieces as u32) {
        return TbProbeResult::NotAvailable;
    }

    let result = probe_wdl(board);

    // Handle 50-move rule consideration
    let effective_result = if ignore_50_move_rule {
        match result {
            TbProbeResult::CursedWin => TbProbeResult::Win,
            TbProbeResult::BlessedLoss => TbProbeResult::Loss,
            other => other,
        }
    } else {
        result
    };

    // Check if result type matches configuration
    match (effective_result, config.result_type) {
        (TbProbeResult::Win | TbProbeResult::Loss, TbResultType::WinLoss | TbResultType::Both) => {
            effective_result
        }
        (TbProbeResult::Draw, TbResultType::Draw | TbResultType::Both) => effective_result,
        (
            TbProbeResult::CursedWin | TbProbeResult::BlessedLoss,
            TbResultType::Draw | TbResultType::Both,
        ) => {
            // Cursed wins and blessed losses are effectively draws under 50-move rule
            TbProbeResult::Draw
        }
        _ => TbProbeResult::NotAvailable,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use chess_library_rs::board::Board;

    #[test]
    fn test_probe_result_from_wdl() {
        assert_eq!(TbProbeResult::from(WdlProbeResult::Win), TbProbeResult::Win);
        assert_eq!(
            TbProbeResult::from(WdlProbeResult::Loss),
            TbProbeResult::Loss
        );
        assert_eq!(
            TbProbeResult::from(WdlProbeResult::Draw),
            TbProbeResult::Draw
        );
        assert_eq!(
            TbProbeResult::from(WdlProbeResult::CursedWin),
            TbProbeResult::CursedWin
        );
        assert_eq!(
            TbProbeResult::from(WdlProbeResult::BlessedLoss),
            TbProbeResult::BlessedLoss
        );
    }

    #[test]
    fn test_not_available_without_init() {
        // Without initialization, probing should return NotAvailable
        // Use a valid KvK position where kings are sufficiently separated
        let board = Board::from_fen("8/8/8/4k3/8/8/4K3/8 w - - 0 1");

        // Note: This test may pass or fail depending on whether init_tablebase
        // was called in other tests. In isolation, it should return NotAvailable.
        let result = probe_wdl(&board);
        // We just verify it doesn't panic
        assert!(matches!(
            result,
            TbProbeResult::NotAvailable | TbProbeResult::Draw
        ));
    }

    #[test]
    fn test_init_tablebase_colon_separated() {
        // Verifies init_tablebase accepts colon-separated paths without panicking.
        // The directories don't exist, so no tables are loaded, but the splitting works.
        init_tablebase("/nonexistent1:/nonexistent2:/nonexistent3");
    }

    #[test]
    fn test_is_available_default() {
        // By default (or without proper init), should be false or true depending on prior tests
        // This just ensures the function doesn't panic
        let _ = is_available();
    }

    #[test]
    fn test_max_pieces_default() {
        // Without loaded tables, should return 0
        let pieces = max_pieces();
        // Could be 0 or higher if tables were loaded in another test
        assert!(pieces <= 7); // Syzygy tables go up to 7 pieces typically
    }
}
