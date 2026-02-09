//! Protocol abstraction for UCI (chess) and USI (shogi).
//!
//! This module provides a unified interface for the differences between
//! the UCI (Universal Chess Interface) and USI (Universal Shogi Interface) protocols.

use crate::types::VariantType;

/// Protocol type - UCI for chess, USI for shogi.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ProtocolType {
    Uci,
    Usi,
}

impl ProtocolType {
    /// Create protocol type from variant.
    pub fn from_variant(variant: VariantType) -> Self {
        match variant {
            VariantType::Standard | VariantType::Frc => ProtocolType::Uci,
            VariantType::Shogi => ProtocolType::Usi,
        }
    }
}

/// Protocol wrapper that provides protocol-specific commands and keywords.
#[derive(Debug, Clone)]
pub struct Protocol {
    protocol_type: ProtocolType,
}

impl Protocol {
    /// Create a new protocol wrapper from variant type.
    pub fn new(variant: VariantType) -> Self {
        Self {
            protocol_type: ProtocolType::from_variant(variant),
        }
    }

    /// Create a UCI protocol wrapper.
    pub fn uci() -> Self {
        Self {
            protocol_type: ProtocolType::Uci,
        }
    }

    /// Create a USI protocol wrapper.
    pub fn usi() -> Self {
        Self {
            protocol_type: ProtocolType::Usi,
        }
    }

    /// Get the protocol type.
    pub fn protocol_type(&self) -> ProtocolType {
        self.protocol_type
    }

    /// Check if this is UCI protocol.
    pub fn is_uci(&self) -> bool {
        self.protocol_type == ProtocolType::Uci
    }

    /// Check if this is USI protocol.
    pub fn is_usi(&self) -> bool {
        self.protocol_type == ProtocolType::Usi
    }

    // ── Initialization commands ──────────────────────────────────────────────

    /// The initialization command ("uci" or "usi").
    pub fn init_cmd(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "uci",
            ProtocolType::Usi => "usi",
        }
    }

    /// The expected response to initialization ("uciok" or "usiok").
    pub fn init_ok(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "uciok",
            ProtocolType::Usi => "usiok",
        }
    }

    /// The new game command ("ucinewgame" or "usinewgame").
    pub fn newgame_cmd(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "ucinewgame",
            ProtocolType::Usi => "isready",
        }
    }

    // ── Position commands ────────────────────────────────────────────────────

    /// The position format keyword ("fen" or "sfen").
    pub fn fen_keyword(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "fen",
            ProtocolType::Usi => "sfen",
        }
    }

    /// Build a position command.
    ///
    /// # Arguments
    /// * `fen` - The FEN/SFEN string (or "startpos" for starting position)
    /// * `moves` - List of moves in UCI/USI notation
    pub fn position_cmd(&self, fen: &str, moves: &[String]) -> String {
        let mut cmd = if fen == "startpos" || fen.is_empty() {
            "position startpos".to_string()
        } else {
            format!("position {} {}", self.fen_keyword(), fen)
        };

        if !moves.is_empty() {
            cmd.push_str(" moves");
            for mv in moves {
                cmd.push(' ');
                cmd.push_str(mv);
            }
        }

        cmd
    }

    // ── Time control ─────────────────────────────────────────────────────────

    /// Get the time parameter name for the first player (White in chess, Black/Sente in shogi).
    ///
    /// In UCI, White moves first, so first player uses "wtime".
    /// In USI, Black (Sente) moves first, so first player uses "btime".
    pub fn first_player_time(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "wtime",
            ProtocolType::Usi => "btime",
        }
    }

    /// Get the time parameter name for the second player (Black in chess, White/Gote in shogi).
    pub fn second_player_time(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "btime",
            ProtocolType::Usi => "wtime",
        }
    }

    /// Get the increment parameter name for the first player.
    pub fn first_player_inc(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "winc",
            ProtocolType::Usi => "binc",
        }
    }

    /// Get the increment parameter name for the second player.
    pub fn second_player_inc(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "binc",
            ProtocolType::Usi => "winc",
        }
    }

    // ── Option name translation ──────────────────────────────────────────────

    /// Translate a UCI option name to the protocol-specific name.
    ///
    /// In USI, some options have different names:
    /// - "Hash" -> "USI_Hash"
    /// - "MultiPV" -> "USI_MultiPV"
    pub fn translate_option_name(&self, name: &str) -> String {
        match self.protocol_type {
            ProtocolType::Uci => name.to_string(),
            ProtocolType::Usi => match name {
                "Hash" => "USI_Hash".to_string(),
                "MultiPV" => "USI_MultiPV".to_string(),
                _ => name.to_string(),
            },
        }
    }

    // ── Color names ──────────────────────────────────────────────────────────

    /// Get the name for the first player (White in chess, Sente in shogi).
    pub fn first_player_name(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "White",
            ProtocolType::Usi => "Sente",
        }
    }

    /// Get the name for the second player (Black in chess, Gote in shogi).
    pub fn second_player_name(&self) -> &'static str {
        match self.protocol_type {
            ProtocolType::Uci => "Black",
            ProtocolType::Usi => "Gote",
        }
    }

    // ── Bestmove parsing ─────────────────────────────────────────────────────

    /// Check if the bestmove string indicates a win declaration (USI only).
    pub fn is_bestmove_win(&self, bestmove: &str) -> bool {
        self.protocol_type == ProtocolType::Usi && bestmove == "win"
    }

    /// Check if the bestmove string indicates resignation (USI only).
    pub fn is_bestmove_resign(&self, bestmove: &str) -> bool {
        self.protocol_type == ProtocolType::Usi && bestmove == "resign"
    }
}

impl Default for Protocol {
    fn default() -> Self {
        Self::uci()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_protocol_from_variant() {
        assert_eq!(
            ProtocolType::from_variant(VariantType::Standard),
            ProtocolType::Uci
        );
        assert_eq!(
            ProtocolType::from_variant(VariantType::Frc),
            ProtocolType::Uci
        );
        assert_eq!(
            ProtocolType::from_variant(VariantType::Shogi),
            ProtocolType::Usi
        );
    }

    #[test]
    fn test_uci_init_commands() {
        let proto = Protocol::uci();
        assert_eq!(proto.init_cmd(), "uci");
        assert_eq!(proto.init_ok(), "uciok");
        assert_eq!(proto.newgame_cmd(), "ucinewgame");
    }

    #[test]
    fn test_usi_init_commands() {
        let proto = Protocol::usi();
        assert_eq!(proto.init_cmd(), "usi");
        assert_eq!(proto.init_ok(), "usiok");
        assert_eq!(proto.newgame_cmd(), "usinewgame");
    }

    #[test]
    fn test_uci_position_command() {
        let proto = Protocol::uci();
        assert_eq!(proto.fen_keyword(), "fen");

        let cmd = proto.position_cmd("startpos", &[]);
        assert_eq!(cmd, "position startpos");

        let cmd = proto.position_cmd(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            &[],
        );
        assert_eq!(
            cmd,
            "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        );

        let cmd = proto.position_cmd("startpos", &["e2e4".to_string(), "e7e5".to_string()]);
        assert_eq!(cmd, "position startpos moves e2e4 e7e5");
    }

    #[test]
    fn test_usi_position_command() {
        let proto = Protocol::usi();
        assert_eq!(proto.fen_keyword(), "sfen");

        let cmd = proto.position_cmd(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
            &[],
        );
        assert_eq!(
            cmd,
            "position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
        );

        let cmd = proto.position_cmd("startpos", &["7g7f".to_string(), "3c3d".to_string()]);
        assert_eq!(cmd, "position startpos moves 7g7f 3c3d");
    }

    #[test]
    fn test_uci_time_params() {
        let proto = Protocol::uci();
        // In UCI, White moves first
        assert_eq!(proto.first_player_time(), "wtime");
        assert_eq!(proto.second_player_time(), "btime");
        assert_eq!(proto.first_player_inc(), "winc");
        assert_eq!(proto.second_player_inc(), "binc");
    }

    #[test]
    fn test_usi_time_params() {
        let proto = Protocol::usi();
        // In USI, Black (Sente) moves first
        assert_eq!(proto.first_player_time(), "btime");
        assert_eq!(proto.second_player_time(), "wtime");
        assert_eq!(proto.first_player_inc(), "binc");
        assert_eq!(proto.second_player_inc(), "winc");
    }

    #[test]
    fn test_uci_option_names() {
        let proto = Protocol::uci();
        assert_eq!(proto.translate_option_name("Hash"), "Hash");
        assert_eq!(proto.translate_option_name("MultiPV"), "MultiPV");
        assert_eq!(proto.translate_option_name("Threads"), "Threads");
    }

    #[test]
    fn test_usi_option_names() {
        let proto = Protocol::usi();
        assert_eq!(proto.translate_option_name("Hash"), "USI_Hash");
        assert_eq!(proto.translate_option_name("MultiPV"), "USI_MultiPV");
        assert_eq!(proto.translate_option_name("Threads"), "Threads");
    }

    #[test]
    fn test_color_names() {
        let uci = Protocol::uci();
        assert_eq!(uci.first_player_name(), "White");
        assert_eq!(uci.second_player_name(), "Black");

        let usi = Protocol::usi();
        assert_eq!(usi.first_player_name(), "Sente");
        assert_eq!(usi.second_player_name(), "Gote");
    }

    #[test]
    fn test_bestmove_special_cases() {
        let uci = Protocol::uci();
        assert!(!uci.is_bestmove_win("win"));
        assert!(!uci.is_bestmove_resign("resign"));

        let usi = Protocol::usi();
        assert!(usi.is_bestmove_win("win"));
        assert!(usi.is_bestmove_resign("resign"));
        assert!(!usi.is_bestmove_win("7g7f"));
        assert!(!usi.is_bestmove_resign("7g7f"));
    }
}
