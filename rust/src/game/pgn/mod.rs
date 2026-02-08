use shakmaty::fen::Fen;
use shakmaty::san::San;
use shakmaty::uci::UciMove;
use shakmaty::{CastlingMode, Chess, Move, Position, Role};

use crate::game::timecontrol::TimeControlLimits;
use crate::types::enums::NotationType;
use crate::types::VariantType;
use crate::types::{GameResult, MatchData, MatchTermination, MoveData, PgnConfig};

const STARTPOS: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const LINE_LENGTH: usize = 80;

/// Builds a PGN string from match data, matching the C++ fastchess output format.
pub struct PgnBuilder;

impl PgnBuilder {
    /// Build a complete PGN game record.
    pub fn build(config: &PgnConfig, data: &MatchData, round: usize) -> String {
        let mut headers: Vec<(String, String)> = Vec::new();
        let result_str = Self::get_result_from_white(&data.players.white);
        let white_name = &data.players.white.config.name;
        let black_name = &data.players.black.config.name;
        let is_frc = data.variant == VariantType::Frc;

        // Standard headers (always emitted)
        headers.push(("Event".into(), config.event_name.clone()));
        headers.push(("Site".into(), config.site.clone()));
        headers.push(("Date".into(), data.date.clone()));
        headers.push(("Round".into(), round.to_string()));
        headers.push(("White".into(), white_name.clone()));
        headers.push(("Black".into(), black_name.clone()));
        headers.push(("Result".into(), result_str.to_string()));

        // FEN/SetUp: emit if non-startpos OR if FRC variant
        if data.fen != STARTPOS || is_frc {
            headers.push(("SetUp".into(), "1".into()));
            headers.push(("FEN".into(), data.fen.clone()));
        }

        // Variant header
        if is_frc {
            headers.push(("Variant".into(), "Chess960".into()));
        }

        // Non-min headers
        if !config.min {
            headers.push(("GameDuration".into(), data.duration.clone()));
            headers.push(("GameStartTime".into(), data.start_time.clone()));
            headers.push(("GameEndTime".into(), data.end_time.clone()));
            headers.push(("PlyCount".into(), data.moves.len().to_string()));
            headers.push((
                "Termination".into(),
                Self::convert_termination(data.termination).into(),
            ));

            let white_tc = &data.players.white.config.limit.tc;
            let black_tc = &data.players.black.config.limit.tc;
            if white_tc == black_tc {
                headers.push(("TimeControl".into(), format_tc(white_tc)));
            } else {
                headers.push(("WhiteTimeControl".into(), format_tc(white_tc)));
                headers.push(("BlackTimeControl".into(), format_tc(black_tc)));
            }
        }

        // Build PGN string
        let mut pgn = String::with_capacity(2048);

        // Emit headers (skip empty values, matching C++ addHeader behavior)
        for (key, value) in &headers {
            if !key.is_empty() && !value.is_empty() {
                pgn.push_str(&format!("[{} \"{}\"]\n", key, value));
            }
        }
        if !headers.is_empty() {
            pgn.push('\n');
        }

        // Determine board state and move numbering from FEN
        let mode = if is_frc {
            CastlingMode::Chess960
        } else {
            CastlingMode::Standard
        };

        let mut board: Option<Chess> = if data.fen.is_empty() || data.fen == STARTPOS {
            Some(Chess::default())
        } else {
            data.fen
                .parse::<Fen>()
                .ok()
                .and_then(|fen| fen.into_position::<Chess>(mode).ok())
        };

        // Compute starting move counter from FEN (matching C++ logic)
        // C++ computes: move_number = int(black_to_move) + 2*fullMoveNumber - 1
        // This is the 1-based ply offset used for move numbering.
        let (black_starts, start_move_counter) = Self::parse_fen_move_info(&data.fen);

        // Check if first move is illegal
        let first_illegal = !data.moves.is_empty() && !data.moves[0].legal;

        if first_illegal {
            // C++ adds: addMove("", reason + ": " + first_move)
            // Which produces: {reason: move} result
            let comment = format!("{}: {}", data.reason, data.moves[0].r#move);
            let pair = format!("{{{}}}", comment);
            pgn.push_str(&pair);
            pgn.push(' ');
            pgn.push_str(result_str);
            pgn.push_str("\n\n");
            return pgn;
        }

        // Generate moves with line wrapping
        let mut current_line_length: usize = 0;

        let append_text = |pgn: &mut String, text: &str, current_line_length: &mut usize| {
            // Check if adding this text exceeds LINE_LENGTH
            // +1 accounts for the space we might add
            if *current_line_length + text.len() + if *current_line_length > 0 { 1 } else { 0 }
                > LINE_LENGTH
            {
                pgn.push('\n');
                *current_line_length = 0;
            }

            if *current_line_length > 0 {
                pgn.push(' ');
                *current_line_length += 1;
            }

            pgn.push_str(text);
            *current_line_length += text.len();
        };

        // @todo fix?
        let notation = if config.min {
            config.notation
        } else {
            config.notation
        };

        let mut move_counter = start_move_counter;
        let move_count = data.moves.len();

        for i in 0..move_count {
            let mv = &data.moves[i];

            // Check if next move is illegal
            let next_illegal = i + 1 < move_count && !data.moves[i + 1].legal;
            let is_last = i + 1 >= move_count || next_illegal;

            let is_white = if !black_starts {
                i % 2 == 0
            } else {
                i % 2 != 0
            };

            let mut pair = String::new();

            // Move notation
            let move_str = Self::convert_move(&mv.r#move, &mut board, notation);
            if !move_str.is_empty() {
                let curr = move_counter.div_ceil(2);
                if is_white {
                    pair = format!("{}. {}", curr, move_str);
                } else if i == 0 {
                    // Black starts: "1... e5"
                    pair = format!("{}... {}", curr, move_str);
                } else {
                    pair = move_str;
                }
            }

            move_counter += 1;

            // Comment
            if !config.min {
                let comment = Self::create_comment(config, data, mv, i, next_illegal, is_last);
                if !comment.is_empty() {
                    if !pair.is_empty() {
                        pair.push(' ');
                    }
                    pair.push('{');
                    pair.push_str(&comment);
                    pair.push('}');
                }
            }

            append_text(&mut pgn, &pair, &mut current_line_length);

            if next_illegal {
                break;
            }
        }

        // Append result terminator
        append_text(&mut pgn, result_str, &mut current_line_length);

        pgn.push_str("\n\n");
        pgn
    }

    /// Parse FEN to determine side to move and starting move counter.
    /// Returns (black_starts, start_move_counter).
    /// The move counter is: int(black_to_move) + 2*fullMoveNumber - 1
    fn parse_fen_move_info(fen: &str) -> (bool, usize) {
        if fen.is_empty() || fen == STARTPOS {
            return (false, 1);
        }

        let parts: Vec<&str> = fen.split_whitespace().collect();
        let side_to_move = parts.get(1).copied().unwrap_or("w");
        let full_move_number: usize = parts.get(5).and_then(|s| s.parse().ok()).unwrap_or(1);

        let black_starts = side_to_move == "b";
        let move_counter = (if black_starts { 1 } else { 0 }) + 2 * full_move_number - 1;

        (black_starts, move_counter)
    }

    /// Convert a UCI move string to the requested notation given a board position.
    fn convert_move(mv_uci: &str, board: &mut Option<Chess>, notation: NotationType) -> String {
        let Some(pos) = board.as_mut() else {
            return mv_uci.to_string();
        };

        match notation {
            NotationType::Uci => {
                // Still need to advance the board
                if let Ok(uci) = mv_uci.parse::<UciMove>() {
                    if let Ok(m) = uci.to_move(pos) {
                        pos.play_unchecked(&m);
                    }
                }
                mv_uci.to_string()
            }
            NotationType::San => {
                if let Ok(uci) = mv_uci.parse::<UciMove>() {
                    if let Ok(m) = uci.to_move(pos) {
                        let san = San::from_move(pos, &m);
                        pos.play_unchecked(&m);
                        return san.to_string();
                    }
                }
                mv_uci.to_string()
            }
            NotationType::Lan => {
                // LAN: Long Algebraic Notation - always includes full origin square
                // e.g., e2e4, Ng1f3, e7e8Q
                if let Ok(uci) = mv_uci.parse::<UciMove>() {
                    if let Ok(m) = uci.to_move(pos) {
                        let lan = Self::move_to_lan(pos, &m);
                        pos.play_unchecked(&m);
                        return lan;
                    }
                }
                mv_uci.to_string()
            }
        }
    }

    /// Convert a Move to Long Algebraic Notation (LAN).
    /// LAN always includes the full origin square, e.g., Ng1f3, e2e4, e7e8Q.
    fn move_to_lan(_pos: &Chess, m: &Move) -> String {
        match m {
            Move::Normal {
                role,
                from,
                to,
                capture,
                promotion,
            } => {
                let mut lan = String::new();

                // Piece symbol (uppercase), except for pawns
                if *role != Role::Pawn {
                    lan.push_str(&role.char().to_uppercase().to_string());
                }

                // Origin square (always included in LAN)
                lan.push_str(&from.to_string());

                // Capture symbol
                if capture.is_some() {
                    lan.push('x');
                }

                // Destination square
                lan.push_str(&to.to_string());

                // Promotion
                if let Some(promo_role) = promotion {
                    lan.push('=');
                    lan.push_str(&promo_role.char().to_uppercase().to_string());
                }

                lan
            }
            Move::EnPassant { from, to, .. } => {
                let mut lan = String::new();
                lan.push_str(&from.to_string());
                lan.push('x');
                lan.push_str(&to.to_string());
                lan
            }
            Move::Castle { king, rook } => {
                // Castling in LAN: use standard O-O / O-O-O notation
                // (C++ chess library does the same)
                if king.file() < rook.file() {
                    "O-O".to_string()
                } else {
                    "O-O-O".to_string()
                }
            }
            Move::Put { role, to } => {
                // Drop notation (for variants like Crazyhouse): Q@f7
                let mut lan = String::new();
                lan.push_str(&role.char().to_uppercase().to_string());
                lan.push('@');
                lan.push_str(&to.to_string());
                lan
            }
        }
    }

    /// Create a comment string for a move, matching C++ format:
    /// `{score_string/depth time, tl=..., latency=..., n=..., ...}`
    ///
    /// For book moves: `{book}`
    /// For the last move: reason is appended to the comment
    fn create_comment(
        config: &PgnConfig,
        data: &MatchData,
        mv: &MoveData,
        move_index: usize,
        next_illegal: bool,
        is_last: bool,
    ) -> String {
        // Book moves get {book}
        if mv.book {
            return "book".to_string();
        }

        let mut parts: Vec<String> = Vec::new();

        // First part: "score_string/depth time" (always present, space-separated)
        let score_depth = format!("{}/{}", mv.score_string, mv.depth);
        let time = format_time(mv.elapsed_millis);
        parts.push(format!("{} {}", score_depth, time));

        // Optional tracked fields
        if config.track_timeleft {
            parts.push(format!("tl={}", format_time(mv.timeleft)));
        }
        if config.track_latency {
            parts.push(format!("latency={}", format_time(mv.latency)));
        }
        if config.track_nodes {
            parts.push(format!("n={}", mv.nodes));
        }
        if config.track_seldepth {
            parts.push(format!("sd={}", mv.seldepth));
        }
        if config.track_nps {
            parts.push(format!("nps={}", mv.nps));
        }
        if config.track_hashfull {
            parts.push(format!("hashfull={}", mv.hashfull));
        }
        if config.track_tbhits {
            parts.push(format!("tbhits={}", mv.tbhits));
        }
        if config.track_pv && !mv.pv.is_empty() {
            parts.push(format!("pv=\"{}\"", mv.pv));
        }

        // Additional info lines
        if !mv.additional_lines.is_empty() {
            let lines: Vec<String> = mv
                .additional_lines
                .iter()
                .map(|line| format!("line=\"{}\"", line))
                .collect();
            parts.push(lines.join(", "));
        }

        // Reason appended to last move's comment
        if is_last {
            let match_str = if next_illegal {
                format!(
                    "{}: {}",
                    data.reason,
                    data.moves.get(move_index + 1).map_or("", |m| &m.r#move)
                )
            } else {
                data.reason.clone()
            };
            if !match_str.is_empty() {
                parts.push(match_str);
            }
        }

        // Join with ", " but skip empty parts
        // C++ uses: ss << first; ((ss << (empty ? "" : ", ") << arg), ...);
        // This means: first part has no comma, subsequent non-empty parts get ", " prefix
        let mut result = String::new();
        for part in &parts {
            if part.is_empty() {
                continue;
            }
            if !result.is_empty() {
                result.push_str(", ");
            }
            result.push_str(part);
        }
        result
    }

    /// Get the PGN result string from the white player's perspective.
    pub fn get_result_from_white(white: &crate::types::PlayerInfo) -> &'static str {
        match white.result {
            GameResult::Win => "1-0",
            GameResult::Lose => "0-1",
            GameResult::Draw => "1/2-1/2",
            GameResult::None => "*",
        }
    }

    /// Convert a MatchTermination to its PGN string representation.
    pub fn convert_termination(term: MatchTermination) -> &'static str {
        match term {
            MatchTermination::Normal => "normal",
            MatchTermination::Adjudication => "adjudication",
            MatchTermination::Timeout => "time forfeit",
            MatchTermination::Disconnect => "abandoned",
            MatchTermination::Stall => "abandoned",
            MatchTermination::IllegalMove => "illegal move",
            MatchTermination::Interrupt => "unterminated",
            MatchTermination::None => "",
        }
    }
}

/// Format milliseconds to seconds with 3 decimal places, matching C++ formatTime.
/// E.g. 1234 -> "1.234s"
fn format_time(millis: i64) -> String {
    format!("{:.3}s", millis as f64 / 1000.0)
}

/// Format a TimeControlLimits for PGN header output.
/// Matches C++ operator<< for TimeControl.
fn format_tc(tc: &TimeControlLimits) -> String {
    if tc.fixed_time > 0 {
        // C++ uses setprecision(8) + noshowpoint
        let secs = tc.fixed_time as f64 / 1000.0;
        return format!("{}/move", format_float_no_trailing_zeros(secs));
    }

    if tc.moves == 0 && tc.time == 0 && tc.increment == 0 {
        return "-".to_string();
    }

    let mut s = String::new();
    if tc.moves > 0 {
        s.push_str(&format!("{}/", tc.moves));
    }
    if tc.time + tc.increment > 0 {
        s.push_str(&format!("{}", tc.time as f64 / 1000.0));
    }
    if tc.increment > 0 {
        s.push_str(&format!("+{}", tc.increment as f64 / 1000.0));
    }
    s
}

/// Format a float removing unnecessary trailing zeros (matching C++ noshowpoint behavior).
fn format_float_no_trailing_zeros(v: f64) -> String {
    // If it's an integer value, show without decimal point
    if v == v.floor() {
        return format!("{}", v as i64);
    }
    // Otherwise trim trailing zeros
    let s = format!("{}", v);
    s
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::engine_config::*;
    use crate::types::match_data::*;

    fn make_config() -> PgnConfig {
        PgnConfig {
            event_name: "Test Tournament".to_string(),
            site: "localhost".to_string(),
            ..PgnConfig::default()
        }
    }

    fn make_player(name: &str, result: GameResult) -> PlayerInfo {
        PlayerInfo {
            config: EngineConfiguration {
                name: name.to_string(),
                ..EngineConfiguration::default()
            },
            result,
        }
    }

    fn make_move(uci: &str, score: &str, depth: i64, elapsed_ms: i64) -> MoveData {
        MoveData {
            r#move: uci.to_string(),
            score_string: score.to_string(),
            depth,
            elapsed_millis: elapsed_ms,
            legal: true,
            ..MoveData::default()
        }
    }

    fn make_book_move(uci: &str) -> MoveData {
        MoveData {
            r#move: uci.to_string(),
            legal: true,
            book: true,
            ..MoveData::default()
        }
    }

    #[test]
    fn test_standard_game_headers() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.duration = "00:00:05".to_string();
        data.start_time = "2025-01-15T10:00:00".to_string();
        data.end_time = "2025-01-15T10:00:05".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("Engine1", GameResult::Win),
            make_player("Engine2", GameResult::Lose),
        );
        data.moves = vec![
            make_move("e2e4", "+0.30", 20, 500),
            make_move("e7e5", "-0.25", 18, 400),
        ];
        data.reason = "White wins".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Check header order and content
        assert!(pgn.contains("[Event \"Test Tournament\"]"));
        assert!(pgn.contains("[Site \"localhost\"]"));
        assert!(pgn.contains("[Date \"2025.01.15\"]"));
        assert!(pgn.contains("[Round \"1\"]"));
        assert!(pgn.contains("[White \"Engine1\"]"));
        assert!(pgn.contains("[Black \"Engine2\"]"));
        assert!(pgn.contains("[Result \"1-0\"]"));
        // Non-min headers
        assert!(pgn.contains("[GameDuration \"00:00:05\"]"));
        assert!(pgn.contains("[GameStartTime \"2025-01-15T10:00:00\"]"));
        assert!(pgn.contains("[GameEndTime \"2025-01-15T10:00:05\"]"));
        assert!(pgn.contains("[PlyCount \"2\"]"));
        assert!(pgn.contains("[Termination \"normal\"]"));
        assert!(pgn.contains("[TimeControl \"-\"]"));
        // No FEN/SetUp for startpos
        assert!(!pgn.contains("[SetUp"));
        assert!(!pgn.contains("[FEN"));
    }

    #[test]
    fn test_min_mode_omits_headers() {
        let mut config = make_config();
        config.min = true;
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![make_move("e2e4", "+0.10", 10, 100)];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Min mode should NOT have these headers
        assert!(!pgn.contains("[GameDuration"));
        assert!(!pgn.contains("[GameStartTime"));
        assert!(!pgn.contains("[GameEndTime"));
        assert!(!pgn.contains("[PlyCount"));
        assert!(!pgn.contains("[Termination"));
        assert!(!pgn.contains("[TimeControl"));
        // Should still have standard headers
        assert!(pgn.contains("[Event"));
        assert!(pgn.contains("[Result"));
    }

    #[test]
    fn test_min_mode_no_comments() {
        let mut config = make_config();
        config.min = true;
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![
            make_move("e2e4", "+0.10", 10, 100),
            make_move("e7e5", "-0.10", 10, 100),
        ];

        let pgn = PgnBuilder::build(&config, &data, 1);

        // No comments in min mode
        assert!(!pgn.contains('{'));
        // Moves should still be present (SAN by default)
        assert!(pgn.contains("1. e4 e5"));
    }

    #[test]
    fn test_annotation_format() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Win),
            make_player("E2", GameResult::Lose),
        );
        data.moves = vec![make_move("e2e4", "+0.50", 20, 1234)];
        data.reason = "White wins".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Should have: {+0.50/20 1.234s, White wins}
        assert!(pgn.contains("{+0.50/20 1.234s, White wins}"));
    }

    #[test]
    fn test_annotation_with_tracked_fields() {
        let mut config = make_config();
        config.track_nodes = true;
        config.track_seldepth = true;
        config.track_timeleft = true;
        config.track_latency = true;

        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Win),
            make_player("E2", GameResult::Lose),
        );
        let mut mv = make_move("e2e4", "+0.50", 20, 1234);
        mv.nodes = 100000;
        mv.seldepth = 25;
        mv.timeleft = 59000;
        mv.latency = 5;
        data.moves = vec![mv];
        data.reason = "White wins".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // C++ format: {score/depth time, tl=..., latency=..., n=..., sd=...}
        assert!(pgn.contains("+0.50/20 1.234s"));
        assert!(pgn.contains("tl=59.000s"));
        assert!(pgn.contains("latency=0.005s"));
        assert!(pgn.contains("n=100000"));
        assert!(pgn.contains("sd=25"));
    }

    #[test]
    fn test_book_move_comment() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![
            make_book_move("e2e4"),
            make_book_move("e7e5"),
            make_move("g1f3", "+0.20", 15, 300),
        ];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Book moves should get {book}
        assert!(pgn.contains("{book}"));
    }

    #[test]
    fn test_pv_inside_comment() {
        let mut config = make_config();
        config.track_pv = true;
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Win),
            make_player("E2", GameResult::Lose),
        );
        let mut mv = make_move("e2e4", "+0.50", 20, 1234);
        mv.pv = "e7e5 g1f3".to_string();
        data.moves = vec![mv];
        data.reason = "White wins".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // PV should be inside the comment as pv="..."
        assert!(pgn.contains("pv=\"e7e5 g1f3\""));
        // Should NOT be RAV format
        assert!(!pgn.contains("(e7e5 g1f3)"));
    }

    #[test]
    fn test_custom_fen_setup_order() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1".to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );

        let pgn = PgnBuilder::build(&config, &data, 1);

        // SetUp should come before FEN (matching C++)
        let setup_pos = pgn.find("[SetUp").unwrap();
        let fen_pos = pgn.find("[FEN").unwrap();
        assert!(setup_pos < fen_pos, "SetUp should come before FEN");
    }

    #[test]
    fn test_black_to_move_fen() {
        let config = make_config();
        let mut data = MatchData::default();
        // FEN where black is to move, fullMoveNumber = 1
        data.fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1".to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![
            make_move("e7e5", "-0.20", 15, 300),
            make_move("g1f3", "+0.25", 16, 350),
        ];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Should start with "1... e5" (black to move)
        assert!(
            pgn.contains("1... e5"),
            "Should have 1... e5 for black-to-move start, got: {}",
            pgn
        );
    }

    #[test]
    fn test_stall_termination() {
        assert_eq!(
            PgnBuilder::convert_termination(MatchTermination::Stall),
            "abandoned"
        );
        assert_eq!(
            PgnBuilder::convert_termination(MatchTermination::Disconnect),
            "abandoned"
        );
    }

    #[test]
    fn test_frc_emits_fen_even_for_startpos() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.variant = VariantType::Frc;
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );

        let pgn = PgnBuilder::build(&config, &data, 1);

        // FRC should always emit SetUp/FEN even for startpos
        assert!(pgn.contains("[SetUp \"1\"]"));
        assert!(pgn.contains("[FEN"));
        assert!(pgn.contains("[Variant \"Chess960\"]"));
    }

    #[test]
    fn test_time_control_formatting() {
        // Fixed time
        let tc = TimeControlLimits {
            fixed_time: 1000,
            ..TimeControlLimits::default()
        };
        assert_eq!(format_tc(&tc), "1/move");

        // Standard TC: 60s + 0.5s increment
        let tc = TimeControlLimits {
            time: 60000,
            increment: 500,
            ..TimeControlLimits::default()
        };
        assert_eq!(format_tc(&tc), "60+0.5");

        // No time control
        let tc = TimeControlLimits::default();
        assert_eq!(format_tc(&tc), "-");

        // Moves per period
        let tc = TimeControlLimits {
            time: 120000,
            moves: 40,
            ..TimeControlLimits::default()
        };
        assert_eq!(format_tc(&tc), "40/120");
    }

    #[test]
    fn test_line_wrapping() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        // Generate enough moves to trigger wrapping
        let moves = vec![
            "e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "b5a4", "g8f6", "e1g1", "f8e7", "f1e1",
            "b7b5", "a4b3", "d7d6", "c2c3", "e8g8",
        ];
        data.moves = moves
            .iter()
            .map(|m| make_move(m, "+0.10", 10, 100))
            .collect();
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Check that the move text section has line breaks
        // (after the empty line that follows headers)
        let move_section = pgn.split("\n\n").nth(1).unwrap_or("");
        let lines: Vec<&str> = move_section.lines().collect();

        // With comments, lines should wrap at ~80 chars
        for line in &lines {
            // Allow some tolerance for the last line with result
            if line.len() > 85 {
                panic!("Line too long ({} chars): {}", line.len(), line);
            }
        }
    }

    #[test]
    fn test_reason_on_last_move() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Adjudication;
        data.players = GamePair::new(
            make_player("E1", GameResult::Win),
            make_player("E2", GameResult::Lose),
        );
        data.moves = vec![
            make_move("e2e4", "+0.30", 20, 500),
            make_move("e7e5", "-9.00", 18, 400),
        ];
        data.reason = "White wins by adjudication".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Reason should be in the LAST move's comment, not a separate comment
        // Last move is e5 (black's move), so its comment should contain the reason
        assert!(pgn.contains("White wins by adjudication}"));
        // Result should follow the last comment directly
        assert!(pgn.contains("1-0\n"));
    }

    #[test]
    fn test_first_illegal_move() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::IllegalMove;
        data.players = GamePair::new(
            make_player("E1", GameResult::Lose),
            make_player("E2", GameResult::Win),
        );
        data.moves = vec![MoveData {
            r#move: "e1e3".to_string(),
            legal: false,
            ..MoveData::default()
        }];
        data.reason = "Illegal move".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // First illegal move: comment is "{reason: move}" then result
        assert!(pgn.contains("{Illegal move: e1e3}"));
        assert!(pgn.contains("0-1"));
    }

    #[test]
    fn test_result_string() {
        assert_eq!(
            PgnBuilder::get_result_from_white(&make_player("E", GameResult::Win)),
            "1-0"
        );
        assert_eq!(
            PgnBuilder::get_result_from_white(&make_player("E", GameResult::Lose)),
            "0-1"
        );
        assert_eq!(
            PgnBuilder::get_result_from_white(&make_player("E", GameResult::Draw)),
            "1/2-1/2"
        );
        assert_eq!(
            PgnBuilder::get_result_from_white(&make_player("E", GameResult::None)),
            "*"
        );
    }

    #[test]
    fn test_format_time() {
        assert_eq!(format_time(0), "0.000s");
        assert_eq!(format_time(1234), "1.234s");
        assert_eq!(format_time(500), "0.500s");
        assert_eq!(format_time(60000), "60.000s");
    }

    #[test]
    fn test_draw_result() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Adjudication;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![
            make_move("e2e4", "+0.00", 20, 500),
            make_move("e7e5", "+0.00", 18, 400),
        ];
        data.reason = "Draw by adjudication".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        assert!(pgn.contains("[Result \"1/2-1/2\"]"));
        assert!(pgn.contains("1/2-1/2\n"));
    }

    #[test]
    fn test_tc_with_split_time_controls() {
        let config = make_config();
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        let mut white = make_player("E1", GameResult::Draw);
        white.config.limit.tc = TimeControlLimits {
            time: 60000,
            increment: 500,
            ..TimeControlLimits::default()
        };
        let mut black = make_player("E2", GameResult::Draw);
        black.config.limit.tc = TimeControlLimits {
            time: 30000,
            increment: 200,
            ..TimeControlLimits::default()
        };
        data.players = GamePair::new(white, black);

        let pgn = PgnBuilder::build(&config, &data, 1);

        // Different TCs should produce split headers
        assert!(pgn.contains("[WhiteTimeControl \"60+0.5\"]"));
        assert!(pgn.contains("[BlackTimeControl \"30+0.2\"]"));
        assert!(!pgn.contains("[TimeControl "));
    }

    /// Test that Chess960 UCI castling moves (e1h1 format) are correctly
    /// converted to SAN O-O notation in PGN output.
    #[test]
    fn test_chess960_castling_to_san() {
        let config = make_config();
        let mut data = MatchData::default();
        // Position after 1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 (white to castle)
        data.fen =
            "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 5".to_string();
        data.variant = VariantType::Frc; // Chess960 mode
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        // Chess960 castling: e1h1 (king from e1 takes rook on h1)
        data.moves = vec![make_move("e1h1", "+0.10", 10, 100)];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // The UCI move e1h1 should become O-O in SAN
        // Note: shakmaty converts castling to O-O/O-O-O in SAN automatically
        assert!(
            pgn.contains("O-O") || pgn.contains("5. O-O"),
            "Chess960 e1h1 should convert to O-O in SAN, got: {}",
            pgn
        );
    }

    /// Test that standard UCI castling moves (e1g1 format) also work.
    #[test]
    fn test_standard_castling_to_san() {
        let config = make_config();
        let mut data = MatchData::default();
        // Same position, standard variant
        data.fen =
            "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 5".to_string();
        data.variant = VariantType::Standard;
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        // Standard castling: e1g1
        data.moves = vec![make_move("e1g1", "+0.10", 10, 100)];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // The UCI move e1g1 should become O-O in SAN
        assert!(
            pgn.contains("O-O") || pgn.contains("5. O-O"),
            "Standard e1g1 should convert to O-O in SAN, got: {}",
            pgn
        );
    }

    /// Test that UCI notation in PGN preserves the original castling format.
    #[test]
    fn test_uci_notation_preserves_castling_format() {
        let mut config = make_config();
        config.notation = NotationType::Uci;
        let mut data = MatchData::default();
        data.fen =
            "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 5".to_string();
        data.variant = VariantType::Frc;
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        // Chess960 castling as UCI
        data.moves = vec![make_move("e1h1", "+0.10", 10, 100)];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // With UCI notation, the move should stay as e1h1
        assert!(
            pgn.contains("e1h1"),
            "UCI notation should preserve e1h1 format, got: {}",
            pgn
        );
    }

    /// Test LAN notation produces full origin square notation.
    #[test]
    fn test_lan_notation() {
        let mut config = make_config();
        config.notation = NotationType::Lan;
        let mut data = MatchData::default();
        data.fen = STARTPOS.to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![
            make_move("e2e4", "+0.10", 10, 100), // Pawn move
            make_move("e7e5", "-0.10", 10, 100), // Pawn move
            make_move("g1f3", "+0.15", 10, 100), // Knight move
            make_move("b8c6", "-0.15", 10, 100), // Knight move
        ];
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // LAN should have full origin squares
        assert!(pgn.contains("e2e4"), "LAN should have e2e4, got: {}", pgn);
        assert!(pgn.contains("e7e5"), "LAN should have e7e5, got: {}", pgn);
        assert!(pgn.contains("Ng1f3"), "LAN should have Ng1f3, got: {}", pgn);
        assert!(pgn.contains("Nb8c6"), "LAN should have Nb8c6, got: {}", pgn);
    }

    /// Test LAN notation for captures.
    #[test]
    fn test_lan_notation_with_captures() {
        let mut config = make_config();
        config.notation = NotationType::Lan;
        let mut data = MatchData::default();
        // Position with a pawn capture available
        data.fen = "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2".to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![make_move("e4d5", "+0.20", 10, 100)]; // Pawn captures
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // LAN should have e4xd5 for pawn capture
        assert!(
            pgn.contains("e4xd5"),
            "LAN should have e4xd5 for capture, got: {}",
            pgn
        );
    }

    /// Test LAN notation for promotions.
    #[test]
    fn test_lan_notation_with_promotion() {
        let mut config = make_config();
        config.notation = NotationType::Lan;
        let mut data = MatchData::default();
        // Position with promotion
        data.fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1".to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Win),
            make_player("E2", GameResult::Lose),
        );
        data.moves = vec![make_move("a7a8q", "+99.00", 10, 100)];
        data.reason = "White wins".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // LAN should have a7a8=Q for promotion
        assert!(
            pgn.contains("a7a8=Q"),
            "LAN should have a7a8=Q for promotion, got: {}",
            pgn
        );
    }

    /// Test LAN notation for castling.
    #[test]
    fn test_lan_notation_castling() {
        let mut config = make_config();
        config.notation = NotationType::Lan;
        let mut data = MatchData::default();
        data.fen =
            "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4".to_string();
        data.date = "2025.01.15".to_string();
        data.termination = MatchTermination::Normal;
        data.players = GamePair::new(
            make_player("E1", GameResult::Draw),
            make_player("E2", GameResult::Draw),
        );
        data.moves = vec![make_move("e1g1", "+0.10", 10, 100)]; // Castling
        data.reason = "".to_string();

        let pgn = PgnBuilder::build(&config, &data, 1);

        // LAN uses O-O for castling (same as SAN)
        assert!(
            pgn.contains("O-O"),
            "LAN should use O-O for castling, got: {}",
            pgn
        );
    }
}
