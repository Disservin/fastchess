use serde::{Deserialize, Serialize};

/// Chess move notation format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum NotationType {
    #[default]
    San,
    Lan,
    Uci,
}

/// Order in which openings are selected from the book.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum OrderType {
    Random,
    #[default]
    Sequential,
}

/// Format of the opening book file.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum FormatType {
    Epd,
    Pgn,
    #[default]
    None,
}

/// Chess variant type.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum VariantType {
    #[default]
    Standard,
    Frc,
}

/// Tournament pairing format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum TournamentType {
    #[default]
    RoundRobin,
    Gauntlet,
}

/// Console output format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum OutputType {
    #[default]
    Fastchess,
    Cutechess,
    None,
}
