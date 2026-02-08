use pgn_reader::{BufferedReader, RawHeader, SanPlus, Skip, Visitor};
use shakmaty::{fen::Fen, uci::UciMove, CastlingMode, Chess, Position};

/// A chess opening position, optionally with a sequence of book moves.
#[derive(Debug, Clone, Default)]
pub struct Opening {
    /// FEN or EPD string for the starting position.
    pub fen_epd: String,
    /// Sequence of moves from that position stored as UCI strings.
    pub moves: Vec<String>,
}

impl Opening {
    pub fn new(fen_epd: &str, moves: Vec<String>) -> Self {
        Self {
            fen_epd: fen_epd.to_string(),
            moves,
        }
    }

    pub fn startpos() -> Self {
        Self {
            fen_epd: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string(),
            moves: Vec::new(),
        }
    }
}

/// Reads openings from an EPD file (one position per line).
pub struct EpdReader {
    openings: Vec<String>,
}

impl EpdReader {
    pub fn new(path: &str) -> Result<Self, String> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| format!("Failed to open EPD file '{}': {}", path, e))?;

        let openings: Vec<String> = content
            .lines()
            .map(|line| line.trim())
            .filter(|line| !line.is_empty())
            .map(|line| line.to_string())
            .collect();

        if openings.is_empty() {
            return Err(format!("No openings found in file: {}", path));
        }

        Ok(Self { openings })
    }

    pub fn get(&self) -> &[String] {
        &self.openings
    }

    pub fn get_mut(&mut self) -> &mut Vec<String> {
        &mut self.openings
    }
}

/// PGN visitor that collects opening moves up to a ply limit.
struct OpeningVisitor {
    fen: String,
    moves: Vec<String>,
    pos: Chess,
    plies_limit: i32,
    ply_count: i32,
    is_frc: bool,
}

impl OpeningVisitor {
    fn new(plies_limit: i32, is_frc: bool) -> Self {
        Self {
            fen: String::new(),
            moves: Vec::new(),
            pos: Chess::default(),
            plies_limit,
            ply_count: 0,
            is_frc,
        }
    }
}

impl Visitor for OpeningVisitor {
    type Result = Opening;

    fn begin_game(&mut self) {
        self.fen = String::new();
        self.moves.clear();
        self.pos = Chess::default();
        self.ply_count = 0;
    }

    fn header(&mut self, key: &[u8], value: RawHeader<'_>) {
        if key == b"FEN" {
            if let Ok(fen_str) = std::str::from_utf8(value.as_bytes()) {
                self.fen = fen_str.to_string();
            }
        }
    }

    fn end_headers(&mut self) -> Skip {
        // Set up the position from the FEN, or use the default start position
        if !self.fen.is_empty() {
            let mode = if self.is_frc {
                CastlingMode::Chess960
            } else {
                CastlingMode::Standard
            };
            if let Ok(fen) = self.fen.parse::<Fen>() {
                if let Ok(pos) = fen.into_position::<Chess>(mode) {
                    self.pos = pos;
                }
            }
        }
        Skip(false)
    }

    fn san(&mut self, san_plus: SanPlus) {
        // Stop collecting moves if we've hit the ply limit
        if self.plies_limit >= 0 && self.ply_count >= self.plies_limit {
            return;
        }

        // Try to parse the SAN move in the current position
        if let Ok(m) = san_plus.san.to_move(&self.pos) {
            // Convert to UCI with correct castling mode and store
            let mode = if self.is_frc {
                CastlingMode::Chess960
            } else {
                CastlingMode::Standard
            };
            let uci = UciMove::from_move(&m, mode);
            self.moves.push(uci.to_string());

            // Advance the position
            self.pos.play_unchecked(&m);
            self.ply_count += 1;
        }
    }

    fn begin_variation(&mut self) -> Skip {
        Skip(true) // skip variations
    }

    fn end_game(&mut self) -> Self::Result {
        let fen = if self.fen.is_empty() {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1".to_string()
        } else {
            self.fen.clone()
        };
        Opening::new(&fen, self.moves.clone())
    }
}

/// Reads openings from a PGN file.
///
/// Each game in the PGN is parsed up to `plies_limit` half-moves.
/// The resulting `Opening` contains the starting FEN and the move list as UCI strings.
pub struct PgnReader {
    openings: Vec<Opening>,
}

impl PgnReader {
    pub fn new(path: &str, plies_limit: i32, is_frc: bool) -> Result<Self, String> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| format!("Failed to open PGN file '{}': {}", path, e))?;

        let mut reader = BufferedReader::new_cursor(content.as_bytes());
        let mut visitor = OpeningVisitor::new(plies_limit, is_frc);
        let mut openings = Vec::new();

        loop {
            match reader.read_game(&mut visitor) {
                Ok(Some(opening)) => {
                    openings.push(opening);
                }
                Ok(None) => break,
                Err(e) => {
                    return Err(format!("Error parsing PGN file '{}': {}", path, e));
                }
            }
        }

        if openings.is_empty() {
            return Err(format!("No openings found in file: {}", path));
        }

        Ok(Self { openings })
    }

    pub fn get(&self) -> &[Opening] {
        &self.openings
    }

    pub fn get_mut(&mut self) -> &mut Vec<Opening> {
        &mut self.openings
    }
}

/// The opening book manager.
///
/// Handles reading, shuffling, rotating, and serving openings to games.
/// Uses interior index tracking to serve openings round-robin style.
#[allow(dead_code)]
pub struct OpeningBook {
    openings: OpeningSource,
    opening_index: usize,
    rounds: usize,
    games: usize,
}

enum OpeningSource {
    None,
    Epd(EpdReader),
    Pgn(PgnReader),
}

impl OpeningBook {
    /// Create an opening book from configuration.
    pub fn new(
        file: &str,
        format: crate::types::FormatType,
        order: crate::types::OrderType,
        plies: i32,
        start: usize,
        rounds: usize,
        games: usize,
        initial_matchcount: usize,
        variant: crate::types::VariantType,
    ) -> Result<Self, String> {
        let offset = start.saturating_sub(1) + initial_matchcount / games;

        let openings = if file.is_empty() {
            OpeningSource::None
        } else {
            match format {
                crate::types::FormatType::Epd => {
                    let mut reader = EpdReader::new(file)?;
                    if order == crate::types::OrderType::Random {
                        shuffle(reader.get_mut());
                    }
                    if offset > 0 {
                        rotate(reader.get_mut(), offset);
                    }
                    truncate(reader.get_mut(), rounds);
                    OpeningSource::Epd(reader)
                }
                crate::types::FormatType::Pgn => {
                    let is_frc = variant == crate::types::VariantType::Frc;
                    let mut reader = PgnReader::new(file, plies, is_frc)?;
                    if order == crate::types::OrderType::Random {
                        shuffle(reader.get_mut());
                    }
                    if offset > 0 {
                        rotate(reader.get_mut(), offset);
                    }
                    truncate(reader.get_mut(), rounds);
                    OpeningSource::Pgn(reader)
                }
                crate::types::FormatType::None => OpeningSource::None,
            }
        };

        Ok(Self {
            openings,
            opening_index: 0,
            rounds,
            games,
        })
    }

    /// Fetch the next opening index, cycling through the book.
    pub fn fetch_id(&mut self) -> Option<usize> {
        let book_size = self.book_size();
        if book_size == 0 {
            return None;
        }
        let idx = self.opening_index;
        self.opening_index += 1;
        Some(idx % book_size)
    }

    /// Get an opening by index.
    pub fn get(&self, idx: Option<usize>) -> Opening {
        match idx {
            None => Opening::startpos(),
            Some(i) => match &self.openings {
                OpeningSource::None => Opening::startpos(),
                OpeningSource::Epd(reader) => {
                    let fen = &reader.get()[i];
                    Opening::new(fen, Vec::new())
                }
                OpeningSource::Pgn(reader) => reader.get()[i].clone(),
            },
        }
    }

    fn book_size(&self) -> usize {
        match &self.openings {
            OpeningSource::None => 0,
            OpeningSource::Epd(reader) => reader.get().len(),
            OpeningSource::Pgn(reader) => reader.get().len(),
        }
    }
}

/// Fisher-Yates shuffle.
fn shuffle<T>(vec: &mut [T]) {
    use rand::Rng;
    let mut rng = rand::thread_rng();
    let len = vec.len();
    for i in 0..len.saturating_sub(1) {
        let j = i + rng.gen_range(0..len - i);
        vec.swap(i, j);
    }
}

/// Rotate a vector by `offset` positions.
fn rotate<T>(vec: &mut [T], offset: usize) {
    if vec.is_empty() {
        return;
    }
    let offset = offset % vec.len();
    vec.rotate_left(offset);
}

/// Truncate a vector to at most `max_len` elements.
fn truncate<T>(vec: &mut Vec<T>, max_len: usize) {
    if vec.len() > max_len {
        vec.truncate(max_len);
        vec.shrink_to_fit();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Test that standard castling produces e1g1/e1c1 format in UCI.
    #[test]
    fn test_standard_castling_uci_format() {
        // A game with kingside castling
        let pgn_content = r#"[Event "Test"]
[Site "?"]
[Date "????.??.??"]
[Round "?"]
[White "W"]
[Black "B"]
[Result "*"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O *
"#;

        // Write to temp file
        let temp_path = std::env::temp_dir().join("test_standard_castling.pgn");
        std::fs::write(&temp_path, pgn_content).unwrap();

        let reader = PgnReader::new(temp_path.to_str().unwrap(), -1, false).unwrap();
        let opening = &reader.get()[0];

        // The 9th move (O-O) should be e1g1 in standard notation
        assert_eq!(opening.moves.len(), 9);
        assert_eq!(opening.moves[8], "e1g1", "Standard castling should be e1g1");

        std::fs::remove_file(&temp_path).ok();
    }

    /// Test that Chess960 castling produces king-takes-rook format (e.g., e1h1).
    #[test]
    fn test_chess960_castling_uci_format() {
        // A Chess960 game with kingside castling (standard start position)
        // In Chess960 notation, O-O from e1 with rook on h1 = e1h1
        let pgn_content = r#"[Event "Test"]
[Site "?"]
[Date "????.??.??"]
[Round "?"]
[White "W"]
[Black "B"]
[Result "*"]
[Variant "Chess960"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O *
"#;

        // Write to temp file
        let temp_path = std::env::temp_dir().join("test_chess960_castling.pgn");
        std::fs::write(&temp_path, pgn_content).unwrap();

        let reader = PgnReader::new(temp_path.to_str().unwrap(), -1, true).unwrap();
        let opening = &reader.get()[0];

        // The 9th move (O-O) should be e1h1 in Chess960 notation
        assert_eq!(opening.moves.len(), 9);
        assert_eq!(
            opening.moves[8], "e1h1",
            "Chess960 castling should be e1h1 (king takes rook)"
        );

        std::fs::remove_file(&temp_path).ok();
    }

    /// Test that queenside castling works correctly in both modes.
    #[test]
    fn test_queenside_castling_both_modes() {
        let pgn_content = r#"[Event "Test"]
[Site "?"]
[Date "????.??.??"]
[Round "?"]
[White "W"]
[Black "B"]
[Result "*"]

1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Bg5 Be7 5. e3 O-O 6. Nf3 Nbd7 7. Qc2 c6 8. Bd3 dxc4 9. Bxc4 b5 10. Bd3 Bb7 11. O-O *
"#;

        // Write to temp files
        let temp_std = std::env::temp_dir().join("test_queenside_std.pgn");
        let temp_frc = std::env::temp_dir().join("test_queenside_frc.pgn");
        std::fs::write(&temp_std, pgn_content).unwrap();
        std::fs::write(&temp_frc, pgn_content).unwrap();

        // Standard mode
        let reader_std = PgnReader::new(temp_std.to_str().unwrap(), -1, false).unwrap();
        let opening_std = &reader_std.get()[0];

        // Black castles O-O at move 10 (ply 9, 0-indexed as 9)
        // White castles O-O at move 21 (ply 21, 0-indexed as 20)
        // Find black's O-O (should be e8g8 in standard)
        assert_eq!(
            opening_std.moves[9], "e8g8",
            "Black O-O should be e8g8 in standard"
        );
        // White's O-O at ply 20
        assert_eq!(
            opening_std.moves[20], "e1g1",
            "White O-O should be e1g1 in standard"
        );

        // Chess960 mode
        let reader_frc = PgnReader::new(temp_frc.to_str().unwrap(), -1, true).unwrap();
        let opening_frc = &reader_frc.get()[0];

        // In Chess960, these become king-takes-rook moves
        assert_eq!(
            opening_frc.moves[9], "e8h8",
            "Black O-O should be e8h8 in Chess960"
        );
        assert_eq!(
            opening_frc.moves[20], "e1h1",
            "White O-O should be e1h1 in Chess960"
        );

        std::fs::remove_file(&temp_std).ok();
        std::fs::remove_file(&temp_frc).ok();
    }

    /// Test non-castling moves are the same in both modes.
    #[test]
    fn test_regular_moves_same_in_both_modes() {
        let pgn_content = r#"[Event "Test"]
[Site "?"]
[Date "????.??.??"]
[Round "?"]
[White "W"]
[Black "B"]
[Result "*"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 *
"#;

        let temp_std = std::env::temp_dir().join("test_regular_std.pgn");
        let temp_frc = std::env::temp_dir().join("test_regular_frc.pgn");
        std::fs::write(&temp_std, pgn_content).unwrap();
        std::fs::write(&temp_frc, pgn_content).unwrap();

        let reader_std = PgnReader::new(temp_std.to_str().unwrap(), -1, false).unwrap();
        let reader_frc = PgnReader::new(temp_frc.to_str().unwrap(), -1, true).unwrap();

        let opening_std = &reader_std.get()[0];
        let opening_frc = &reader_frc.get()[0];

        // All moves should be identical (no castling)
        assert_eq!(opening_std.moves, opening_frc.moves);
        assert_eq!(
            opening_std.moves,
            vec!["e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6"]
        );

        std::fs::remove_file(&temp_std).ok();
        std::fs::remove_file(&temp_frc).ok();
    }

    /// Test Opening::startpos returns standard starting position.
    #[test]
    fn test_opening_startpos() {
        let opening = Opening::startpos();
        assert_eq!(
            opening.fen_epd,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        );
        assert!(opening.moves.is_empty());
    }

    /// Test Opening::new creates opening with given FEN and moves.
    #[test]
    fn test_opening_new() {
        let moves = vec!["e2e4".to_string(), "e7e5".to_string()];
        let opening = Opening::new("custom_fen", moves.clone());
        assert_eq!(opening.fen_epd, "custom_fen");
        assert_eq!(opening.moves, moves);
    }
}
