/// UCI and SAN move parsing/formatting — 1:1 port of `uci.hpp`.
use crate::variants::chessport::board::{Board, GameResult};
use crate::variants::chessport::chess_move::Move;
use crate::variants::chessport::coords::{File, Rank, Square};
use crate::variants::chessport::movegen;
use crate::variants::chessport::movelist::Movelist;
use crate::variants::chessport::piece::PieceType;

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SanParseError(pub String);

impl std::fmt::Display for SanParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "SAN parse error: {}", self.0)
    }
}

impl std::error::Error for SanParseError {}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AmbiguousMoveError(pub String);

impl std::fmt::Display for AmbiguousMoveError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Ambiguous move: {}", self.0)
    }
}

impl std::error::Error for AmbiguousMoveError {}

// ---------------------------------------------------------------------------
// Internal SAN parse info
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
struct SanMoveInformation {
    from_file: File,
    from_rank: Rank,
    promotion: PieceType,
    from: Square,
    to: Square,
    piece: PieceType,
    castling_short: bool,
    castling_long: bool,
    capture: bool,
}

impl Default for SanMoveInformation {
    fn default() -> Self {
        SanMoveInformation {
            from_file: File::NO_FILE,
            from_rank: Rank::NO_RANK,
            promotion: PieceType::NONE,
            from: Square::NO_SQ,
            to: Square::NO_SQ,
            piece: PieceType::NONE,
            castling_short: false,
            castling_long: false,
            capture: false,
        }
    }
}

// ---------------------------------------------------------------------------
// SAN info parser
// ---------------------------------------------------------------------------

fn parse_san_info(san: &str) -> Result<SanMoveInformation, SanParseError> {
    if san.len() < 2 {
        return Err(SanParseError(format!(
            "Failed to parse san. At step 0: {}",
            san
        )));
    }

    let bytes = san.as_bytes();
    let mut info = SanMoveInformation::default();
    let mut throw_error = false;

    let is_rank = |c: u8| c >= b'1' && c <= b'8';
    let is_file = |c: u8| c >= b'a' && c <= b'h';

    // Castling
    if bytes[0] == b'O' || bytes[0] == b'0' {
        let castle_char = bytes[0];
        info.piece = PieceType::KING;
        // expect "O-O" then optionally "-O" for long
        // san[3..] after stripping "O-O"
        let rest = if san.len() >= 3 { &san[3..] } else { "" };
        info.castling_short = rest.is_empty() || (rest.len() >= 1 && rest.as_bytes()[0] != b'-');
        info.castling_long =
            rest.len() >= 2 && rest.as_bytes()[0] == b'-' && rest.as_bytes()[1] == castle_char;
        return Ok(info);
    }

    // set to 1 to skip piece type offset
    let mut index: usize = 1;

    if is_file(bytes[0]) {
        index = 0; // pawn: no piece prefix
        info.piece = PieceType::PAWN;
    } else {
        info.piece = piece_type_from_san_char(bytes[0] as char);
        if info.piece == PieceType::NONE {
            throw_error = true;
        }
    }

    let mut file_to = File::NO_FILE;
    let mut rank_to = Rank::NO_RANK;

    // check if san starts with a file
    if index < bytes.len() && is_file(bytes[index]) {
        info.from_file = File::from_char(bytes[index] as char);
        index += 1;
    }

    // check if san starts with a rank
    if index < bytes.len() && is_rank(bytes[index]) {
        info.from_rank = Rank::from_char(bytes[index] as char);
        index += 1;
    }

    // skip capture sign
    if index < bytes.len() && bytes[index] == b'x' {
        info.capture = true;
        index += 1;
    }

    // to file
    if index < bytes.len() && is_file(bytes[index]) {
        file_to = File::from_char(bytes[index] as char);
        index += 1;
    }

    // to rank
    if index < bytes.len() && is_rank(bytes[index]) {
        rank_to = Rank::from_char(bytes[index] as char);
        index += 1;
    }

    // promotion
    if index < bytes.len() && bytes[index] == b'=' {
        index += 1;
        if index < bytes.len() {
            info.promotion = piece_type_from_san_char(bytes[index] as char);
            if info.promotion == PieceType::KING
                || info.promotion == PieceType::PAWN
                || info.promotion == PieceType::NONE
            {
                throw_error = true;
            }
            index += 1;
        } else {
            throw_error = true;
        }
    }

    // for simple moves like Nf3, e4 — all info is in from_file/from_rank; move to to_file/to_rank
    if file_to == File::NO_FILE && rank_to == Rank::NO_RANK {
        file_to = info.from_file;
        rank_to = info.from_rank;
        info.from_file = File::NO_FILE;
        info.from_rank = Rank::NO_RANK;
    }

    // non-capturing pawns stay on the same file
    if info.piece == PieceType::PAWN && info.from_file == File::NO_FILE && !info.capture {
        info.from_file = file_to;
    }

    if file_to != File::NO_FILE && rank_to != Rank::NO_RANK {
        info.to = Square::new(file_to, rank_to);
    } else {
        throw_error = true;
    }

    let _ = index; // suppress unused warning

    if throw_error {
        return Err(SanParseError(format!(
            "Failed to parse san. At step 1: {}",
            san
        )));
    }

    if info.from_file != File::NO_FILE && info.from_rank != Rank::NO_RANK {
        info.from = Square::new(info.from_file, info.from_rank);
    }

    Ok(info)
}

/// Parse an uppercase piece-type letter as used in SAN (N/B/R/Q/K).
fn piece_type_from_san_char(c: char) -> PieceType {
    match c {
        'N' => PieceType::KNIGHT,
        'B' => PieceType::BISHOP,
        'R' => PieceType::ROOK,
        'Q' => PieceType::QUEEN,
        'K' => PieceType::KING,
        // Promotions are written with lowercase in some contexts
        'n' => PieceType::KNIGHT,
        'b' => PieceType::BISHOP,
        'r' => PieceType::ROOK,
        'q' => PieceType::QUEEN,
        _ => PieceType::NONE,
    }
}

// ---------------------------------------------------------------------------
// Ambiguity resolution helpers (for moveToSan / moveToLan)
// ---------------------------------------------------------------------------

fn is_identifiable_by_file(moves: &Movelist, mv: Move, file: File) -> bool {
    for m in moves.iter() {
        if *m == mv || m.to() != mv.to() {
            continue;
        }
        if m.from().file() == file {
            return false;
        }
    }
    true
}

fn is_identifiable_by_rank(moves: &Movelist, mv: Move, rank: Rank) -> bool {
    for m in moves.iter() {
        if *m == mv || m.to() != mv.to() {
            continue;
        }
        if m.from().rank() == rank {
            return false;
        }
    }
    true
}

fn resolve_ambiguity(board: &Board, mv: Move, piece_type: PieceType, out: &mut String) {
    let mut moves = Movelist::new();
    movegen::legalmoves(
        &mut moves,
        board,
        movegen::MoveGenType::All,
        1 << piece_type.as_u8() as u32,
    );

    let mut need_file = false;
    let mut need_rank = false;
    let mut has_ambiguous = false;

    for m in moves.iter() {
        if *m != mv && m.to() == mv.to() {
            has_ambiguous = true;

            if is_identifiable_by_file(&moves, mv, mv.from().file()) {
                need_file = true;
                break;
            }

            if is_identifiable_by_rank(&moves, mv, mv.from().rank()) {
                need_rank = true;
                break;
            }
        }
    }

    if need_file {
        out.push((b'a' + mv.from().file() as u8) as char);
    }
    if need_rank {
        out.push((b'1' + mv.from().rank() as u8) as char);
    }

    // can't distinguish by file or rank alone — use full square
    if has_ambiguous && !need_file && !need_rank {
        out.push((b'a' + mv.from().file() as u8) as char);
        out.push((b'1' + mv.from().rank() as u8) as char);
    }
}

fn append_check_symbol(board: &mut Board, out: &mut String) {
    let (_, result) = board.is_game_over();
    if result == GameResult::Lose {
        out.push('#');
    } else if board.in_check() {
        out.push('+');
    }
}

fn move_to_rep<const LAN: bool>(board: &Board, mv: Move) -> String {
    let mut out = String::new();
    let mut board = board.clone();

    // castling
    if mv.type_of() == Move::CASTLING {
        out = if mv.to() > mv.from() {
            "O-O".to_owned()
        } else {
            "O-O-O".to_owned()
        };
        board.make_move(mv, false);
        append_check_symbol(&mut board, &mut out);
        return out;
    }

    let pt = board.at(mv.from()).piece_type();
    let is_capture = board.at(mv.to()) != crate::variants::chessport::piece::Piece::NONE
        || mv.type_of() == Move::ENPASSANT;

    debug_assert!(pt != PieceType::NONE);

    if pt != PieceType::PAWN {
        out.push(pt.as_char().to_ascii_uppercase());
    }

    if LAN {
        out.push((b'a' + mv.from().file() as u8) as char);
        out.push((b'1' + mv.from().rank() as u8) as char);
    } else if pt == PieceType::PAWN {
        if is_capture {
            out.push((b'a' + mv.from().file() as u8) as char);
        }
    } else {
        resolve_ambiguity(&board, mv, pt, &mut out);
    }

    if is_capture {
        out.push('x');
    }

    out.push((b'a' + mv.to().file() as u8) as char);
    out.push((b'1' + mv.to().rank() as u8) as char);

    if mv.type_of() == Move::PROMOTION {
        out.push('=');
        out.push(mv.promotion_type().as_char().to_ascii_uppercase());
    }

    board.make_move(mv, false);
    append_check_symbol(&mut board, &mut out);

    out
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Convert a move to its UCI string (e.g. `"e2e4"`, `"e7e8q"`).
///
/// For non-chess960 castling the king destination is remapped to g1/c1 (or g8/c8).
pub fn move_to_uci(mv: Move, chess960: bool) -> String {
    let from_sq = mv.from();
    let mut to_sq = mv.to();

    if !chess960 && mv.type_of() == Move::CASTLING {
        // remap rook-square to king destination
        let file = if to_sq > from_sq {
            File::FILE_G
        } else {
            File::FILE_C
        };
        to_sq = Square::new(file, from_sq.rank());
    }

    let mut s = String::with_capacity(5);
    s.push((b'a' + from_sq.file() as u8) as char);
    s.push((b'1' + from_sq.rank() as u8) as char);
    s.push((b'a' + to_sq.file() as u8) as char);
    s.push((b'1' + to_sq.rank() as u8) as char);

    if mv.type_of() == Move::PROMOTION {
        s.push(mv.promotion_type().as_char());
    }

    s
}

/// Convert a UCI string to an internal `Move` given the current board position.
pub fn uci_to_move(board: &Board, uci: &str) -> Move {
    if uci.len() < 4 {
        return Move::from_raw(Move::NO_MOVE);
    }

    let b = uci.as_bytes();
    let source = Square::new(File::from_char(b[0] as char), Rank::from_char(b[1] as char));
    let mut target = Square::new(File::from_char(b[2] as char), Rank::from_char(b[3] as char));

    if !source.is_valid() || !target.is_valid() {
        return Move::from_raw(Move::NO_MOVE);
    }

    let pt = board.at(source).piece_type();

    // chess960 castling: king captures own rook
    if board.chess960()
        && pt == PieceType::KING
        && board.at(target).piece_type() == PieceType::ROOK
        && board.at(target).color() == board.side_to_move()
    {
        return Move::make::<{ Move::CASTLING }>(source, target, PieceType::KNIGHT);
    }

    // standard castling: king moves 2 squares
    if !board.chess960() && pt == PieceType::KING && Square::distance(target, source) == 2 {
        let file = if target > source {
            File::FILE_H
        } else {
            File::FILE_A
        };
        target = Square::new(file, source.rank());
        return Move::make::<{ Move::CASTLING }>(source, target, PieceType::KNIGHT);
    }

    // en passant
    if pt == PieceType::PAWN && target == board.enpassant_sq() {
        return Move::make::<{ Move::ENPASSANT }>(source, target, PieceType::KNIGHT);
    }

    // promotion
    if pt == PieceType::PAWN && uci.len() == 5 && Square::back_rank(target, !board.side_to_move()) {
        let promo = piece_type_from_san_char(b[4] as char);
        if promo == PieceType::NONE || promo == PieceType::KING || promo == PieceType::PAWN {
            return Move::from_raw(Move::NO_MOVE);
        }
        return Move::make::<{ Move::PROMOTION }>(source, target, promo);
    }

    if uci.len() == 4 {
        Move::make::<{ Move::NORMAL }>(source, target, PieceType::KNIGHT)
    } else {
        Move::from_raw(Move::NO_MOVE)
    }
}

/// Convert a move to SAN (Standard Algebraic Notation).
pub fn move_to_san(board: &Board, mv: Move) -> String {
    move_to_rep::<false>(board, mv)
}

/// Convert a move to LAN (Long Algebraic Notation).
pub fn move_to_lan(board: &Board, mv: Move) -> String {
    move_to_rep::<true>(board, mv)
}

/// Parse SAN and return the matching legal `Move`.
/// Returns an error if the SAN is invalid or the move is ambiguous.
pub fn parse_san(board: &Board, san: &str) -> Result<Move, SanParseError> {
    let mut moves = Movelist::new();
    parse_san_with_movelist(board, san, &mut moves)
}

/// Same as [`parse_san`] but re-uses an existing `Movelist` allocation.
pub fn parse_san_with_movelist(
    board: &Board,
    san: &str,
    moves: &mut Movelist,
) -> Result<Move, SanParseError> {
    if san.is_empty() {
        return Ok(Move::from_raw(Move::NO_MOVE));
    }

    let pt_to_pgt = |pt: PieceType| 1u32 << pt.as_u8() as u32;

    let info = parse_san_info(san)?;

    if info.capture {
        movegen::legalmoves(
            moves,
            board,
            movegen::MoveGenType::Capture,
            pt_to_pgt(info.piece),
        );
    } else {
        movegen::legalmoves(
            moves,
            board,
            movegen::MoveGenType::Quiet,
            pt_to_pgt(info.piece),
        );
    }

    // castling
    if info.castling_short || info.castling_long {
        for mv in moves.iter() {
            if mv.type_of() == Move::CASTLING {
                let is_king_side = mv.to() > mv.from();
                if (info.castling_short && is_king_side) || (info.castling_long && !is_king_side) {
                    return Ok(*mv);
                }
            }
        }
        return Err(SanParseError(format!(
            "Failed to parse san. At step 2: {} {}",
            san,
            board.get_fen(true)
        )));
    }

    let mut matching_move = Move::from_raw(Move::NO_MOVE);
    let mut found_match = false;

    for mv in moves.iter() {
        if mv.to() != info.to || mv.type_of() == Move::CASTLING {
            continue;
        }

        if info.promotion != PieceType::NONE {
            if mv.type_of() != Move::PROMOTION
                || info.promotion != mv.promotion_type()
                || mv.from().file() != info.from_file
            {
                continue;
            }
        } else if mv.type_of() == Move::ENPASSANT {
            if mv.from().file() != info.from_file {
                continue;
            }
        } else if info.from != Square::NO_SQ {
            if mv.from() != info.from {
                continue;
            }
        } else if info.from_rank != Rank::NO_RANK || info.from_file != File::NO_FILE {
            if (info.from_file != File::NO_FILE && mv.from().file() != info.from_file)
                || (info.from_rank != Rank::NO_RANK && mv.from().rank() != info.from_rank)
            {
                continue;
            }
        }

        if found_match {
            return Err(SanParseError(format!(
                "Ambiguous san: {} in {}",
                san,
                board.get_fen(true)
            )));
        }

        matching_move = *mv;
        found_match = true;
    }

    if !found_match {
        return Err(SanParseError(format!(
            "Failed to parse san, illegal move: {} {}",
            san,
            board.get_fen(true)
        )));
    }

    Ok(matching_move)
}

/// Returns `true` if `s` looks like a valid UCI move string.
pub fn is_uci_move(s: &str) -> bool {
    let b = s.as_bytes();
    let is_digit = |c: u8| c >= b'1' && c <= b'8';
    let is_file = |c: u8| c >= b'a' && c <= b'h';
    let is_promo = |c: u8| matches!(c, b'n' | b'b' | b'r' | b'q');

    if b.len() < 4 {
        return false;
    }
    if b.len() > 5 {
        return false;
    }

    let base = is_file(b[0]) && is_digit(b[1]) && is_file(b[2]) && is_digit(b[3]);
    if b.len() == 5 {
        base && is_promo(b[4])
    } else {
        base
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::variants::chessport::board::Board;

    // helpers
    fn board(fen: &str) -> Board {
        Board::from_fen(fen)
    }

    // -----------------------------------------------------------------------
    // Original tests
    // -----------------------------------------------------------------------

    #[test]
    fn test_move_to_uci_startpos_e4() {
        let board = Board::new();
        let m = uci_to_move(&board, "e2e4");
        assert!(m.raw() != Move::NO_MOVE);
        assert_eq!(move_to_uci(m, false), "e2e4");
    }

    #[test]
    fn test_parse_san_e4() {
        let board = Board::new();
        let m = parse_san(&board, "e4").expect("e4 should parse");
        assert_eq!(move_to_uci(m, false), "e2e4");
    }

    #[test]
    fn test_parse_san_nf3() {
        let board = Board::new();
        let m = parse_san(&board, "Nf3").expect("Nf3 should parse");
        assert_eq!(move_to_uci(m, false), "g1f3");
    }

    #[test]
    fn test_move_to_san_e4() {
        let board = Board::new();
        let m = uci_to_move(&board, "e2e4");
        assert_eq!(move_to_san(&board, m), "e4");
    }

    #[test]
    fn test_move_to_san_nf3() {
        let board = Board::new();
        let m = uci_to_move(&board, "g1f3");
        assert_eq!(move_to_san(&board, m), "Nf3");
    }

    #[test]
    fn test_castling_san() {
        let fen = "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
        let b = Board::from_fen(fen);
        let m = parse_san(&b, "O-O").expect("O-O should parse");
        assert_eq!(move_to_san(&b, m), "O-O");
    }

    #[test]
    fn test_uci_move_roundtrip() {
        let board = Board::new();
        for uci_str in &["e2e4", "d2d4", "g1f3", "b1c3"] {
            let m = uci_to_move(&board, uci_str);
            assert!(m.raw() != Move::NO_MOVE, "failed for {}", uci_str);
            assert_eq!(&move_to_uci(m, false), uci_str);
        }
    }

    #[test]
    fn test_is_uci_move() {
        assert!(is_uci_move("e2e4"));
        assert!(is_uci_move("e7e8q"));
        assert!(!is_uci_move("e4"));
        assert!(!is_uci_move("e2e4x"));
    }

    // -----------------------------------------------------------------------
    // Ported from C++ TEST_SUITE("SAN Parser")
    // -----------------------------------------------------------------------

    #[test]
    fn test_ambiguous_pawn_capture() {
        let b = board("rnbqkbnr/ppp1p1pp/3p1p2/4N3/8/3P4/PPPKPPPP/R1BQ1BNR b kq - 1 7");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F6, Square::SQ_E5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "fxe5");
        assert_eq!(parse_san(&b, "fxe5").unwrap(), m);
    }

    #[test]
    fn test_ambiguous_pawn_ep_capture() {
        let b = board("rnbqkbnr/pppppp1p/8/5PpP/8/8/PPPPP2P/RNBQKBNR w KQkq g6 0 2");
        let m = Move::make::<{ Move::ENPASSANT }>(Square::SQ_F5, Square::SQ_G6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "fxg6");
        assert_eq!(parse_san(&b, "fxg6").unwrap(), m);
    }

    #[test]
    fn test_ambiguous_knight_move() {
        let b = board("rnbqkbnr/ppp3pp/3ppp2/8/8/3P1N1N/PPPKPPPP/R1BQ1B1R w kq - 1 8");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F3, Square::SQ_G5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Nfg5");
        assert_eq!(parse_san(&b, "Nfg5").unwrap(), m);
    }

    #[test]
    fn test_ambiguous_rook_move_with_check() {
        let b = board("4k3/8/8/8/8/8/2R3R1/2K5 w - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C2, Square::SQ_E2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rce2+");
        assert_eq!(parse_san(&b, "Rce2+").unwrap(), m);
    }

    #[test]
    fn test_ambiguous_rook_move_with_checkmate() {
        let b = board("4k3/8/8/8/8/8/2KR1R2/3R1R2 w - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_D2, Square::SQ_E2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rde2#");
        assert_eq!(parse_san(&b, "Rde2#").unwrap(), m);
    }

    #[test]
    fn test_knight_move() {
        let b = board("rnbqkbnr/ppp1p1pp/3p1p2/8/8/3P1N2/PPPKPPPP/R1BQ1BNR w kq - 0 7");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F3, Square::SQ_G5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Ng5");
        assert_eq!(parse_san(&b, "Ng5").unwrap(), m);
    }

    #[test]
    fn test_bishop_move() {
        let b = board("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F1, Square::SQ_C4, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bc4");
        assert_eq!(parse_san(&b, "Bc4").unwrap(), m);
    }

    #[test]
    fn test_rook_move() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPP1PP/R3KR2 w Qkq - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F1, Square::SQ_F7, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rxf7");
        assert_eq!(parse_san(&b, "Rxf7").unwrap(), m);
    }

    #[test]
    fn test_queen_move() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPP1PP/R3KQ1R w KQkq - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F1, Square::SQ_F7, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qxf7+");
        assert_eq!(parse_san(&b, "Qxf7+").unwrap(), m);
    }

    #[test]
    fn test_king_move() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E1, Square::SQ_F1, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Kf1");
        assert_eq!(parse_san(&b, "Kf1").unwrap(), m);
    }

    #[test]
    fn test_king_castling_short() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 17");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_H1, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "O-O");
        assert_eq!(parse_san(&b, "O-O").unwrap(), m);
    }

    #[test]
    fn test_king_castling_long() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_A1, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "O-O-O");
        assert_eq!(parse_san(&b, "O-O-O").unwrap(), m);
    }

    #[test]
    fn test_king_castling_short_with_zero() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 17");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_H1, PieceType::KNIGHT);
        assert_eq!(parse_san(&b, "0-0").unwrap(), m);
    }

    #[test]
    fn test_king_castling_long_with_zero() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_A1, PieceType::KNIGHT);
        assert_eq!(parse_san(&b, "0-0-0").unwrap(), m);
    }

    #[test]
    fn test_king_castling_short_with_annotation() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 17");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_H1, PieceType::KNIGHT);
        assert_eq!(parse_san(&b, "0-0+?!").unwrap(), m);
    }

    #[test]
    fn test_king_castling_long_with_annotation() {
        let b = board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_A1, PieceType::KNIGHT);
        assert_eq!(parse_san(&b, "0-0-0+?!").unwrap(), m);
    }

    #[test]
    fn test_queen_capture_ambiguity() {
        let b = board("3k4/8/4b3/8/2Q3Q1/8/8/3K4 w - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_E6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qcxe6");
        assert_eq!(parse_san(&b, "Qcxe6").unwrap(), m);
    }

    #[test]
    fn test_rook_ambiguity() {
        let b = board("3k4/8/8/R7/8/8/8/R3K3 w - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_A1, Square::SQ_A3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R1a3");
        assert_eq!(parse_san(&b, "R1a3").unwrap(), m);
    }

    #[test]
    fn test_rook_capture_ambiguity() {
        let b = board("2r3k1/4nn2/pq1p1pp1/3Pp3/1pN1P1P1/1P1Q4/P1r1NP2/1K1R3R b - - 2 19");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C8, Square::SQ_C4, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R8xc4");
        assert_eq!(parse_san(&b, "R8xc4").unwrap(), m);
    }

    #[test]
    fn test_knight_capture_ambiguity() {
        let b = board("r5k1/5p2/2n2B1p/4P3/p1n3PN/8/P4PK1/1R6 b - - 2 28");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C6, Square::SQ_E5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "N6xe5");
        assert_eq!(parse_san(&b, "N6xe5").unwrap(), m);
    }

    #[test]
    fn test_pawn_capture_promotion_ambiguity() {
        let b = board("2k2n2/4P1P1/8/8/8/8/8/2K5 w - - 0 1");
        let m = Move::make::<{ Move::PROMOTION }>(Square::SQ_E7, Square::SQ_F8, PieceType::QUEEN);
        assert_eq!(move_to_san(&b, m), "exf8=Q+");
        assert_eq!(parse_san(&b, "exf8=Q+").unwrap(), m);
    }

    #[test]
    fn test_pawn_push() {
        let b = Board::new();
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_E4, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "e4");
        assert_eq!(parse_san(&b, "e4").unwrap(), m);
    }

    #[test]
    fn test_pawn_promotion() {
        let b = board("8/Pk6/8/5p2/8/8/8/2K5 w - - 1 781");
        let m = Move::make::<{ Move::PROMOTION }>(Square::SQ_A7, Square::SQ_A8, PieceType::QUEEN);
        assert_eq!(move_to_san(&b, m), "a8=Q+");
        assert_eq!(parse_san(&b, "a8=Q+").unwrap(), m);
    }

    #[test]
    fn test_knight_ambiguity() {
        let b = board("8/8/5K2/2N3P1/3N3n/4k3/3N4/7r w - - 59 97");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_D4, Square::SQ_B3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "N4b3");
        assert_eq!(parse_san(&b, "N4b3").unwrap(), m);
    }

    #[test]
    fn test_knight_capture_ambiguity_2() {
        let b = board("8/8/5K2/2N3P1/3N3n/1b2k3/3N4/7r w - - 59 97");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_D4, Square::SQ_B3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "N4xb3");
        assert_eq!(parse_san(&b, "N4xb3").unwrap(), m);
    }

    #[test]
    fn test_knight_ambiguity_2() {
        let b = board("2N1N3/p7/6k1/1p6/2N1N3/2R5/R3Q1P1/2R3K1 w - - 5 55");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E4, Square::SQ_D6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Ne4d6");
        assert_eq!(parse_san(&b, "Ne4d6").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_D6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Nc4d6");
        assert_eq!(parse_san(&b, "Nc4d6").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C8, Square::SQ_D6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Nc8d6");
        assert_eq!(parse_san(&b, "Nc8d6").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E8, Square::SQ_D6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Ne8d6");
        assert_eq!(parse_san(&b, "Ne8d6").unwrap(), m);
    }

    #[test]
    fn test_knight_capture_check_ambiguity() {
        let b = board("2N1N3/p4k2/3q4/1p6/2N1N3/2R5/R3Q1P1/2R3K1 w - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E4, Square::SQ_D6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Ne4xd6+");
        assert_eq!(parse_san(&b, "Ne4xd6+").unwrap(), m);
    }

    #[test]
    fn test_bishop_ambiguity() {
        let b = board("4k3/8/8/8/2B1B3/8/2B1BK2/8 w - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C2, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bc2d3");
        assert_eq!(parse_san(&b, "Bc2d3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bc4d3");
        assert_eq!(parse_san(&b, "Bc4d3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Be2d3");
        assert_eq!(parse_san(&b, "Be2d3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Be4d3");
        assert_eq!(parse_san(&b, "Be4d3").unwrap(), m);
    }

    #[test]
    fn test_rook_ambiguity_multi() {
        let b = board("k5r1/8/8/8/2R1R3/8/2R1RK2/8 w - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C2, Square::SQ_C3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R2c3");
        assert_eq!(parse_san(&b, "R2c3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C2, Square::SQ_D2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rcd2");
        assert_eq!(parse_san(&b, "Rcd2").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_E3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R2e3");
        assert_eq!(parse_san(&b, "R2e3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_D2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Red2");
        assert_eq!(parse_san(&b, "Red2").unwrap(), m);
    }

    #[test]
    fn test_queen_ambiguity() {
        let b = board("1k4r1/8/8/8/2Q1Q3/8/2Q1QK2/8 w - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C2, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qc2d3");
        assert_eq!(parse_san(&b, "Qc2d3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qc4d3");
        assert_eq!(parse_san(&b, "Qc4d3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qe2d3");
        assert_eq!(parse_san(&b, "Qe2d3").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qe4d3");
        assert_eq!(parse_san(&b, "Qe4d3").unwrap(), m);
    }

    #[test]
    fn test_queen_ambiguity_2() {
        let b = board("5q1k/4P3/7q/1Q1pQ3/3P4/1KN5/8/5q2 b - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F8, Square::SQ_F6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Q8f6");
        assert_eq!(parse_san(&b, "Q8f6").unwrap(), m);
    }

    #[test]
    fn test_queen_ambiguity_3() {
        let b = board("5q1k/4P3/7q/1Q1pQ3/3P3q/1KN5/8/5q2 b - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_H4, Square::SQ_F6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Q4f6");
        assert_eq!(parse_san(&b, "Q4f6").unwrap(), m);
    }

    #[test]
    fn test_queen_ambiguity_4() {
        let b = board("5q1k/4P3/7q/1Q1pQ3/3P1q1q/1KN5/8/5q2 b - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_H4, Square::SQ_F6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qh4f6");
        assert_eq!(parse_san(&b, "Qh4f6").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F4, Square::SQ_F6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qf4f6");
        assert_eq!(parse_san(&b, "Qf4f6").unwrap(), m);
    }

    #[test]
    fn test_rook_ambiguity_b_file() {
        let b = board("1r5k/5Rp1/5p1p/4Nb1P/2B5/2P1R1P1/3r1PK1/1r6 b - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B1, Square::SQ_B2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R1b2");
        assert_eq!(parse_san(&b, "R1b2").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_D2, Square::SQ_B2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rdb2");
        assert_eq!(parse_san(&b, "Rdb2").unwrap(), m);
    }

    #[test]
    fn test_rook_ambiguity_2() {
        let b = board("1r5k/5Rp1/5p1p/1r2Nb1P/2B5/2P1R1P1/3r1PK1/8 b - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B5, Square::SQ_B2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rbb2");
        assert_eq!(parse_san(&b, "Rbb2").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B5, Square::SQ_B6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R5b6");
        assert_eq!(parse_san(&b, "R5b6").unwrap(), m);
    }

    #[test]
    fn test_rook_ambiguity_3() {
        let b = board("1r5k/5Rp1/5p1p/1r2Nb1P/2B5/2P1R1P1/5PK1/8 b - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B5, Square::SQ_B2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rb2");
        assert_eq!(parse_san(&b, "Rb2").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B5, Square::SQ_B6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R5b6");
        assert_eq!(parse_san(&b, "R5b6").unwrap(), m);
    }

    #[test]
    fn test_bishop_ambiguity_2() {
        let b = board("2Bqkb1r/4p2p/8/8/2B5/p7/4B3/RNBQKBNR w KQk - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_A6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "B4a6");
        assert_eq!(parse_san(&b, "B4a6").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bcd3");
        assert_eq!(parse_san(&b, "Bcd3").unwrap(), m);
    }

    #[test]
    fn test_bishop_ambiguity_3() {
        let b = board("3qkb1r/4p2p/8/8/2B5/p7/4B3/RNBQKBNR w KQk - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_A6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Ba6");
        assert_eq!(parse_san(&b, "Ba6").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bcd3");
        assert_eq!(parse_san(&b, "Bcd3").unwrap(), m);
    }

    #[test]
    fn test_bishop_capture_mating_ambiguity() {
        let b = board("7r/4pqbp/5bkp/6nn/2B5/p2r4/4B3/RNBQKBR1 w Q - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bcxd3#");
        assert_eq!(parse_san(&b, "Bcxd3#").unwrap(), m);
    }

    #[test]
    fn test_pinned_queen_ambiguity() {
        let b = board("1Q3q1k/4P3/8/3p1N1q/1Q1P4/2N5/2K5/5q2 b - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_F1, Square::SQ_F5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qfxf5+");
        assert_eq!(parse_san(&b, "Qfxf5+").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_H5, Square::SQ_F5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Qhxf5+");
        assert_eq!(parse_san(&b, "Qhxf5+").unwrap(), m);
    }

    #[test]
    fn test_pinned_rook_ambiguity() {
        let b = board("1r6/5Rp1/5p1p/1r2Nb1P/2B5/2PR2P1/3r1PK1/3k4 b - - 0 1");

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B5, Square::SQ_B2, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Rb2");
        assert_eq!(parse_san(&b, "Rb2").unwrap(), m);

        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_B5, Square::SQ_B6, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "R5b6");
        assert_eq!(parse_san(&b, "R5b6").unwrap(), m);
    }

    #[test]
    fn test_pinned_bishop_ambiguity() {
        let b = board("2r4r/4pqbp/5bkp/6nn/2B5/p2r4/2K1B3/RNBQ1BR1 w - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_D3, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Bxd3#");
        assert_eq!(parse_san(&b, "Bxd3#").unwrap(), m);
    }

    #[test]
    fn test_pinned_knight_ambiguity() {
        let b = board("r1k5/5p2/2n2B1p/2R1P3/p1n3PN/8/P4PK1/1R6 b - - 0 1");
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_C4, Square::SQ_E5, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "Nxe5");
        assert_eq!(parse_san(&b, "Nxe5").unwrap(), m);
    }

    #[test]
    fn test_parse_no_move() {
        let b = Board::new();
        assert_eq!(parse_san(&b, "").unwrap(), Move::from_raw(Move::NO_MOVE));
    }

    #[test]
    fn test_should_err_on_ambiguous_move() {
        let b = board("8/8/6K1/4k3/4N3/p4r2/N3N3/8 w - - 3 82");
        let result = parse_san(&b, "Nec3");
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert!(
            err.0.contains("Ambiguous san: Nec3"),
            "unexpected error: {}",
            err
        );
    }

    #[test]
    fn test_should_err_for_illegal_move() {
        let b = board("8/8/6K1/4k3/4N3/p4r2/N3N3/8 w - - 3 82");
        let result = parse_san(&b, "Nec4");
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert!(
            err.0.contains("Failed to parse san, illegal move: Nec4"),
            "unexpected error: {}",
            err
        );
    }

    #[test]
    fn test_checkmate_castle_has_hash() {
        let b = board("RRR5/8/8/8/8/8/PPPPPP2/k3K2R w K - 0 1");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_H1, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "O-O#");
        assert_eq!(parse_san(&b, "O-O#").unwrap(), m);
    }

    #[test]
    fn test_check_castle_has_plus() {
        let b = board("8/8/8/8/8/8/PPPPPP2/k3K2R w K - 0 1");
        let m = Move::make::<{ Move::CASTLING }>(Square::SQ_E1, Square::SQ_H1, PieceType::KNIGHT);
        assert_eq!(move_to_san(&b, m), "O-O+");
        assert_eq!(parse_san(&b, "O-O+").unwrap(), m);
    }
}
