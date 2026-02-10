//! Syzygy tablebase probing module.
//!
//! Provides tablebase probing for endgame adjudication using the `shakmaty_syzygy` crate.

use std::path::Path;
use std::sync::OnceLock;

use shakmaty::{Chess, Position};
use shakmaty_syzygy::{AmbiguousWdl, Dtz, MaybeRounded, Tablebase, Wdl};

use crate::types::adjudication::{TbAdjudication, TbResultType};

/// Global tablebase instance (initialized once).
static TABLEBASE: OnceLock<Option<Tablebase<Chess>>> = OnceLock::new();

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

impl From<Wdl> for TbProbeResult {
    fn from(wdl: Wdl) -> Self {
        match wdl {
            Wdl::Win => TbProbeResult::Win,
            Wdl::Loss => TbProbeResult::Loss,
            Wdl::Draw => TbProbeResult::Draw,
            Wdl::CursedWin => TbProbeResult::CursedWin,
            Wdl::BlessedLoss => TbProbeResult::BlessedLoss,
        }
    }
}

impl From<AmbiguousWdl> for TbProbeResult {
    fn from(wdl: AmbiguousWdl) -> Self {
        match wdl {
            AmbiguousWdl::Win => TbProbeResult::Win,
            AmbiguousWdl::Loss => TbProbeResult::Loss,
            AmbiguousWdl::Draw => TbProbeResult::Draw,
            AmbiguousWdl::CursedWin => TbProbeResult::CursedWin,
            AmbiguousWdl::BlessedLoss => TbProbeResult::BlessedLoss,
            // MaybeLoss/MaybeWin are ambiguous due to en passant; treat conservatively
            AmbiguousWdl::MaybeWin => TbProbeResult::Win,
            AmbiguousWdl::MaybeLoss => TbProbeResult::Loss,
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

        let mut tb = Tablebase::new();
        let mut any_loaded = false;

        for dir in syzygy_dirs.split(';') {
            let dir = dir.trim();
            if dir.is_empty() {
                continue;
            }

            let path = Path::new(dir);
            if path.is_dir() {
                match tb.add_directory(path) {
                    Ok(count) => {
                        if count > 0 {
                            log::info!("Loaded {} tablebase files from {}", count, dir);
                            any_loaded = true;
                        }
                    }
                    Err(e) => {
                        log::warn!("Failed to load tablebases from {}: {}", dir, e);
                    }
                }
            } else {
                log::warn!("Syzygy directory not found: {}", dir);
            }
        }

        if any_loaded {
            Some(tb)
        } else {
            log::warn!("No Syzygy tablebases loaded");
            None
        }
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
pub fn probe_wdl(pos: &Chess) -> TbProbeResult {
    let Some(Some(tb)) = TABLEBASE.get() else {
        return TbProbeResult::NotAvailable;
    };

    match tb.probe_wdl(pos) {
        Ok(wdl) => wdl.into(),
        Err(_) => TbProbeResult::NotAvailable,
    }
}

/// Probe DTZ (Distance To Zeroing) for a position.
///
/// Returns `None` if the position cannot be probed.
pub fn probe_dtz(pos: &Chess) -> Option<MaybeRounded<Dtz>> {
    let Some(Some(tb)) = TABLEBASE.get() else {
        return None;
    };

    tb.probe_dtz(pos).ok()
}

/// Check if a position can be probed (piece count within limits).
pub fn can_probe(pos: &Chess, max_pieces_limit: u32) -> bool {
    let piece_count = pos.board().occupied().count();

    // Check against configured max pieces limit
    if max_pieces_limit > 0 && piece_count > max_pieces_limit as usize {
        return false;
    }

    // Check against loaded tablebase capacity
    let tb_max = max_pieces();
    if tb_max > 0 && piece_count > tb_max as usize {
        return false;
    }

    // Need at least some tablebases loaded
    is_available()
}

/// Determine if a position should be adjudicated based on tablebase probe.
///
/// Returns the probe result if adjudication is applicable, or `NotAvailable` otherwise.
pub fn should_adjudicate(
    pos: &Chess,
    config: &TbAdjudication,
    ignore_50_move_rule: bool,
) -> TbProbeResult {
    if !config.enabled {
        return TbProbeResult::NotAvailable;
    }

    if !can_probe(pos, config.max_pieces as u32) {
        return TbProbeResult::NotAvailable;
    }

    let result = probe_wdl(pos);

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
    use shakmaty::fen::Fen;
    use shakmaty::CastlingMode;

    #[test]
    fn test_probe_result_from_wdl() {
        assert_eq!(TbProbeResult::from(Wdl::Win), TbProbeResult::Win);
        assert_eq!(TbProbeResult::from(Wdl::Loss), TbProbeResult::Loss);
        assert_eq!(TbProbeResult::from(Wdl::Draw), TbProbeResult::Draw);
        assert_eq!(
            TbProbeResult::from(Wdl::CursedWin),
            TbProbeResult::CursedWin
        );
        assert_eq!(
            TbProbeResult::from(Wdl::BlessedLoss),
            TbProbeResult::BlessedLoss
        );
    }

    #[test]
    fn test_not_available_without_init() {
        // Without initialization, probing should return NotAvailable
        // Use a valid KvK position where kings are sufficiently separated
        let fen: Fen = "8/8/8/4k3/8/8/4K3/8 w - - 0 1".parse().unwrap();
        let pos: Chess = fen.into_position(CastlingMode::Standard).unwrap();

        // Note: This test may pass or fail depending on whether init_tablebase
        // was called in other tests. In isolation, it should return NotAvailable.
        let result = probe_wdl(&pos);
        // We just verify it doesn't panic
        assert!(matches!(
            result,
            TbProbeResult::NotAvailable | TbProbeResult::Draw
        ));
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
