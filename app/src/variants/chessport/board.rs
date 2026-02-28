use crate::variants::chessport::attacks;
use crate::variants::chessport::bitboard::Bitboard;
use crate::variants::chessport::chess_move::Move;
use crate::variants::chessport::color::Color;
use crate::variants::chessport::coords::{File, Rank, Square};
use crate::variants::chessport::movegen;
use crate::variants::chessport::piece::{Piece, PieceType};
use crate::variants::chessport::zobrist::Zobrist;

// ---------------------------------------------------------------------------
// CastlingRights
// ---------------------------------------------------------------------------

/// Side for castling (king-side = true, queen-side = false).
/// Encoded in methods as `is_king_side: bool`.

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct CastlingRights {
    /// `rooks[color_idx][side_idx]`  where side_idx 0 = king-side, 1 = queen-side.
    rooks: [[File; 2]; 2],
}

impl CastlingRights {
    pub const KING_SIDE: bool = true;
    pub const QUEEN_SIDE: bool = false;

    #[inline]
    pub fn new() -> Self {
        CastlingRights {
            rooks: [[File::NoFile; 2]; 2],
        }
    }

    /// Index used for `rooks[color][side]`.
    #[inline]
    fn side_idx(is_king_side: bool) -> usize {
        if is_king_side {
            0
        } else {
            1
        }
    }

    #[inline]
    pub fn set(&mut self, color: Color, is_king_side: bool, rook_file: File) {
        self.rooks[color.index()][Self::side_idx(is_king_side)] = rook_file;
    }

    /// Clear all castling rights.
    #[inline]
    pub fn clear_all(&mut self) {
        self.rooks = [[File::NoFile; 2]; 2];
    }

    /// Clear one specific right, returns `color_idx * 2 + side_idx` for Zobrist.
    #[inline]
    pub fn clear_one(&mut self, color: Color, is_king_side: bool) -> usize {
        self.rooks[color.index()][Self::side_idx(is_king_side)] = File::NoFile;
        color.index() * 2 + Self::side_idx(is_king_side)
    }

    /// Clear all rights for a color.
    #[inline]
    pub fn clear_color(&mut self, color: Color) {
        self.rooks[color.index()] = [File::NoFile; 2];
    }

    #[inline]
    pub fn has(&self, color: Color, is_king_side: bool) -> bool {
        self.rooks[color.index()][Self::side_idx(is_king_side)] != File::NoFile
    }

    #[inline]
    pub fn has_color(&self, color: Color) -> bool {
        self.has(color, true) || self.has(color, false)
    }

    #[inline]
    pub fn is_empty(&self) -> bool {
        !self.has_color(Color::White) && !self.has_color(Color::Black)
    }

    #[inline]
    pub fn get_rook_file(&self, color: Color, is_king_side: bool) -> File {
        self.rooks[color.index()][Self::side_idx(is_king_side)]
    }

    /// 4-bit index for Zobrist: bit0=WK, bit1=WQ, bit2=BK, bit3=BQ.
    #[inline]
    pub fn hash_index(&self) -> usize {
        (self.has(Color::White, true) as usize)
            | ((self.has(Color::White, false) as usize) << 1)
            | ((self.has(Color::Black, true) as usize) << 2)
            | ((self.has(Color::Black, false) as usize) << 3)
    }

    /// Returns `true` (king-side) if `sq > pred`, else `false` (queen-side).
    #[inline]
    pub fn closest_side_sq(sq: Square, pred: Square) -> bool {
        sq > pred
    }

    /// Same logic but for files.
    #[inline]
    pub fn closest_side_file(file: File, pred: File) -> bool {
        file > pred
    }
}

impl Default for CastlingRights {
    fn default() -> Self {
        Self::new()
    }
}

// ---------------------------------------------------------------------------
// State (saved before each makeMove)
// ---------------------------------------------------------------------------

#[derive(Clone, Debug)]
struct State {
    hash: u64,
    castling: CastlingRights,
    enpassant: Square,
    half_moves: u8,
    captured_piece: Piece,
}

// ---------------------------------------------------------------------------
// GameResult enums
// ---------------------------------------------------------------------------

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum GameResult {
    Win,
    Lose,
    Draw,
    None,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum GameResultReason {
    Checkmate,
    Stalemate,
    InsufficientMaterial,
    FiftyMoveRule,
    ThreefoldRepetition,
    None,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum CheckType {
    NoCheck,
    DirectCheck,
    DiscoveryCheck,
}

// ---------------------------------------------------------------------------
// PackedBoard (24-byte compact representation)
// ---------------------------------------------------------------------------

pub type PackedBoard = [u8; 24];

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------

#[derive(Clone, Debug)]
pub struct Board {
    pub(crate) pieces_bb_: [Bitboard; 6],
    pub(crate) occ_bb_: [Bitboard; 2],
    pub(crate) board_: [Piece; 64],
    pub(crate) key_: u64,
    pub(crate) cr_: CastlingRights,
    pub(crate) plies_: u16,
    pub(crate) stm_: Color,
    pub(crate) ep_sq_: Square,
    pub(crate) hfm_: u8,
    pub(crate) chess960_: bool,
    pub(crate) castling_path: [[Bitboard; 2]; 2],
    prev_states_: Vec<State>,
    original_fen_: String,
}

// ---------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------

pub const STARTPOS: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// ---------------------------------------------------------------------------
// Board implementation
// ---------------------------------------------------------------------------

impl Board {
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    pub fn new() -> Self {
        let mut b = Board::empty();
        b.prev_states_.reserve(256);
        // Initialize with startpos (ignore parse error for now)
        let _ = b.set_fen_internal(STARTPOS, false);
        b
    }

    pub fn from_fen(fen: &str) -> Self {
        let mut b = Board::empty();
        b.prev_states_.reserve(256);
        let _ = b.set_fen_internal(fen, false);
        b
    }

    pub fn from_xfen(xfen: &str) -> Self {
        let mut b = Board::empty();
        b.prev_states_.reserve(256);
        b.chess960_ = true;
        let _ = b.set_fen_common(
            xfen,
            |board, castling| board.parse_xfen_castling(castling),
            false,
        );
        b
    }

    fn empty() -> Self {
        Board {
            pieces_bb_: [Bitboard(0); 6],
            occ_bb_: [Bitboard(0); 2],
            board_: [Piece::NONE; 64],
            key_: 0,
            cr_: CastlingRights::new(),
            plies_: 0,
            stm_: Color::White,
            ep_sq_: Square::NO_SQ,
            hfm_: 0,
            chess960_: false,
            castling_path: [[Bitboard(0); 2]; 2],
            prev_states_: Vec::new(),
            original_fen_: String::new(),
        }
    }

    // -----------------------------------------------------------------------
    // Reset
    // -----------------------------------------------------------------------

    fn reset(&mut self) {
        self.pieces_bb_ = [Bitboard(0); 6];
        self.occ_bb_ = [Bitboard(0); 2];
        self.board_ = [Piece::NONE; 64];
        self.stm_ = Color::White;
        self.ep_sq_ = Square::NO_SQ;
        self.hfm_ = 0;
        self.plies_ = 1;
        self.key_ = 0;
        self.cr_.clear_all();
        self.prev_states_.clear();
    }

    // -----------------------------------------------------------------------
    // Internal piece placement (no Zobrist update)
    // -----------------------------------------------------------------------

    pub(crate) fn place_piece(&mut self, piece: Piece, sq: Square) {
        debug_assert!(self.board_[sq.index()] == Piece::NONE);
        let pt = piece.piece_type();
        let color = piece.color();
        self.pieces_bb_[pt.as_u8() as usize].set(sq.0);
        self.occ_bb_[color.index()].set(sq.0);
        self.board_[sq.index()] = piece;
    }

    pub(crate) fn remove_piece(&mut self, piece: Piece, sq: Square) {
        debug_assert!(self.board_[sq.index()] == piece);
        let pt = piece.piece_type();
        let color = piece.color();
        self.pieces_bb_[pt.as_u8() as usize].clear_bit(sq.0);
        self.occ_bb_[color.index()].clear_bit(sq.0);
        self.board_[sq.index()] = Piece::NONE;
    }

    // -----------------------------------------------------------------------
    // FEN parsing helpers
    // -----------------------------------------------------------------------

    /// Parse an integer from a string slice (strips trailing ';').
    fn parse_int(s: &str) -> Option<i32> {
        let s = s.trim_end_matches(';').trim();
        if s.is_empty() {
            return None;
        }
        s.parse::<i32>().ok()
    }

    /// Split a string_view by delimiter, returning up to N parts.
    fn split_fen(s: &str, delimiter: char, n: usize) -> Vec<&str> {
        let mut parts = Vec::with_capacity(n);
        let mut start = 0;
        for (i, c) in s.char_indices() {
            if c == delimiter {
                if parts.len() < n - 1 {
                    parts.push(&s[start..i]);
                    start = i + 1;
                }
                if parts.len() == n - 1 {
                    break;
                }
            }
        }
        parts.push(&s[start..]);
        parts
    }

    // -----------------------------------------------------------------------
    // Castling helpers
    // -----------------------------------------------------------------------

    /// Find a rook on the given side of the king.
    fn find_rook_on_side(&self, color: Color, is_king_side: bool) -> File {
        let king_sq = self.king_sq(color);
        if king_sq == Square::NO_SQ {
            return File::NoFile;
        }
        let sq_corner = if is_king_side {
            Square::SQ_H1.relative_square(color)
        } else {
            Square::SQ_A1.relative_square(color)
        };

        if is_king_side {
            let start_idx = king_sq.0 as i32 + 1;
            let end_idx = sq_corner.0 as i32;
            let mut sq_idx = start_idx;
            while sq_idx <= end_idx {
                let sq = Square(sq_idx as u8);
                if self.at_type(sq) == PieceType::ROOK && self.at(sq).color() == color {
                    return sq.file();
                }
                sq_idx += 1;
            }
        } else {
            let start_idx = king_sq.0 as i32 - 1;
            let end_idx = sq_corner.0 as i32;
            let mut sq_idx = start_idx;
            while sq_idx >= end_idx {
                let sq = Square(sq_idx as u8);
                if self.at_type(sq) == PieceType::ROOK && self.at(sq).color() == color {
                    return sq.file();
                }
                sq_idx -= 1;
            }
        }

        File::NoFile
    }

    /// Find the outermost rook on a given side of the king.
    fn outer_rook_file(&self, color: Color, is_king_side: bool) -> File {
        let king_sq = self.king_sq(color);
        if king_sq == Square::NO_SQ {
            return File::NoFile;
        }
        let back_rank = king_sq.rank();
        let mut best: Option<File> = None;

        for f_u8 in 0u8..8 {
            let f = File::from_u8(f_u8);
            let sq = Square::new(f, back_rank);
            if self.at_type(sq) != PieceType::ROOK || self.at(sq).color() != color {
                continue;
            }
            if is_king_side {
                if f <= king_sq.file() {
                    continue;
                }
                if best.is_none() || f > best.unwrap() {
                    best = Some(f);
                }
            } else {
                if f >= king_sq.file() {
                    continue;
                }
                if best.is_none() || f < best.unwrap() {
                    best = Some(f);
                }
            }
        }

        best.unwrap_or(File::NoFile)
    }

    fn has_rook_on_file(&self, color: Color, file: File) -> bool {
        let king_sq = self.king_sq(color);
        if king_sq == Square::NO_SQ {
            return false;
        }
        let rook_sq = Square::new(file, king_sq.rank());
        self.at_type(rook_sq) == PieceType::ROOK && self.at(rook_sq).color() == color
    }

    fn apply_castling_right(&mut self, color: Color, is_king_side: bool, fallback: File) -> bool {
        let file = if self.has_rook_on_file(color, fallback) {
            fallback
        } else {
            let f = self.find_rook_on_side(color, is_king_side);
            if f == File::NoFile {
                return false;
            }
            f
        };
        self.cr_.set(color, is_king_side, file);
        true
    }

    fn parse_fen_castling(&mut self, castling: &str) {
        for c in castling.chars() {
            if c == '-' {
                break;
            }
            if !self.chess960_ {
                match c {
                    'K' => {
                        self.apply_castling_right(Color::White, true, File::FileH);
                    }
                    'Q' => {
                        self.apply_castling_right(Color::White, false, File::FileA);
                    }
                    'k' => {
                        self.apply_castling_right(Color::Black, true, File::FileH);
                    }
                    'q' => {
                        self.apply_castling_right(Color::Black, false, File::FileA);
                    }
                    _ => {}
                }
            } else {
                // chess960 FEN castling
                let color = if c.is_uppercase() {
                    Color::White
                } else {
                    Color::Black
                };
                let king_sq = self.king_sq(color);

                match c.to_ascii_uppercase() {
                    'K' => {
                        let file = self.find_rook_on_side(color, true);
                        if file != File::NoFile {
                            self.cr_.set(color, true, file);
                        }
                    }
                    'Q' => {
                        let file = self.find_rook_on_side(color, false);
                        if file != File::NoFile {
                            self.cr_.set(color, false, file);
                        }
                    }
                    _ => {
                        let file = File::from_char(c);
                        if file == File::NoFile || king_sq == Square::NO_SQ {
                            continue;
                        }
                        let is_king_side = CastlingRights::closest_side_file(file, king_sq.file());
                        self.cr_.set(color, is_king_side, file);
                    }
                }
            }
        }
    }

    fn parse_xfen_castling(&mut self, castling: &str) {
        for c in castling.chars() {
            if c == '-' {
                break;
            }
            let color = if c.is_uppercase() {
                Color::White
            } else {
                Color::Black
            };
            let king_sq = self.king_sq(color);
            if king_sq == Square::NO_SQ {
                return;
            }

            match c.to_ascii_uppercase() {
                'K' => {
                    let file = self.outer_rook_file(color, true);
                    if file != File::NoFile {
                        self.cr_.set(color, true, file);
                    }
                }
                'Q' => {
                    let file = self.outer_rook_file(color, false);
                    if file != File::NoFile {
                        self.cr_.set(color, false, file);
                    }
                }
                _ => {
                    let file = File::from_char(c);
                    if file == File::NoFile || file == king_sq.file() {
                        continue;
                    }
                    if !self.has_rook_on_file(color, file) {
                        continue;
                    }
                    let is_king_side = CastlingRights::closest_side_file(file, king_sq.file());
                    self.cr_.set(color, is_king_side, file);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Core FEN parsing
    // -----------------------------------------------------------------------

    fn set_fen_common<F>(&mut self, fen: &str, parse_castling: F, require_kings: bool) -> bool
    where
        F: Fn(&mut Board, &str),
    {
        self.original_fen_ = fen.to_string();

        self.reset();

        let fen = fen.trim_start();
        if fen.is_empty() {
            return false;
        }

        let parts = Self::split_fen(fen, ' ', 6);
        let position = parts.get(0).copied().unwrap_or("");
        let move_right = parts.get(1).copied().unwrap_or("w");
        let castling = parts.get(2).copied().unwrap_or("-");
        let en_passant = parts.get(3).copied().unwrap_or("-");
        let half_move = parts.get(4).copied().unwrap_or("0");
        let full_move = parts.get(5).copied().unwrap_or("1");

        if position.is_empty() {
            return false;
        }
        if move_right != "w" && move_right != "b" {
            return false;
        }

        self.hfm_ = Self::parse_int(half_move).unwrap_or(0).max(0) as u8;
        let full_move_n = Self::parse_int(full_move).unwrap_or(1).max(1) as u16;
        self.plies_ = full_move_n * 2 - 2;

        // Parse en passant (validation happens after piece placement)
        if en_passant != "-" {
            if !Square::is_valid_string(en_passant) {
                return false;
            }
            self.ep_sq_ = Square::from_str(en_passant);
            if self.ep_sq_ == Square::NO_SQ {
                return false;
            }
        }

        self.stm_ = if move_right == "w" {
            Color::White
        } else {
            Color::Black
        };

        if self.stm_ == Color::Black {
            self.plies_ += 1;
        } else {
            self.key_ ^= Zobrist::side_to_move();
        }

        // Parse piece placement
        let mut square: i32 = 56;
        for c in position.chars() {
            if c.is_ascii_digit() {
                square += (c as i32) - ('0' as i32);
            } else if c == '/' {
                square -= 16;
            } else {
                let p = Piece::from_char(c);
                if p == Piece::NONE {
                    return false;
                }
                if square < 0 || square >= 64 {
                    return false;
                }
                let sq = Square(square as u8);
                if self.board_[sq.index()] != Piece::NONE {
                    return false;
                }
                self.place_piece(p, sq);
                self.key_ ^= Zobrist::piece(p, sq);
                square += 1;
            }
        }

        if require_kings {
            if self.pieces_pt_color(PieceType::KING, Color::White).0 == 0
                || self.pieces_pt_color(PieceType::KING, Color::Black).0 == 0
            {
                return false;
            }
        }

        parse_castling(self, castling);

        // Validate ep square rank
        if self.ep_sq_ != Square::NO_SQ {
            let valid_rank = (self.ep_sq_.rank() == Rank::Rank3 && self.stm_ == Color::Black)
                || (self.ep_sq_.rank() == Rank::Rank6 && self.stm_ == Color::White);
            if !valid_rank {
                self.ep_sq_ = Square::NO_SQ;
            }
        }

        // Validate ep square legality
        if self.ep_sq_ != Square::NO_SQ {
            let ep = self.ep_sq_;
            let valid = movegen::is_ep_square_valid(self, ep);
            if !valid {
                self.ep_sq_ = Square::NO_SQ;
            } else {
                self.key_ ^= Zobrist::enpassant(ep.file());
            }
        }

        self.key_ ^= Zobrist::castling(self.cr_.hash_index());

        debug_assert!(self.key_ == self.zobrist());

        // Compute castling_path
        self.castling_path = [[Bitboard(0); 2]; 2];
        self.recompute_castling_path();

        true
    }

    fn recompute_castling_path(&mut self) {
        for color in [Color::White, Color::Black] {
            let king_from = self.king_sq(color);
            if king_from == Square::NO_SQ {
                continue;
            }
            for is_king_side in [true, false] {
                if !self.cr_.has(color, is_king_side) {
                    continue;
                }
                let rook_file = self.cr_.get_rook_file(color, is_king_side);
                let rook_from = Square::new(rook_file, king_from.rank());
                let king_to = Square::castling_king_square(is_king_side, color);
                let rook_to = Square::castling_rook_square(is_king_side, color);

                let path = (movegen::between(rook_from, rook_to)
                    | movegen::between(king_from, king_to))
                    & !(Bitboard::from_square(king_from) | Bitboard::from_square(rook_from));

                self.castling_path[color.index()][is_king_side as usize] = path;
            }
        }
    }

    fn set_fen_internal(&mut self, fen: &str, is_xfen: bool) -> bool {
        if is_xfen {
            self.set_fen_common(
                fen,
                |board, castling| board.parse_xfen_castling(castling),
                false,
            )
        } else {
            self.set_fen_common(
                fen,
                |board, castling| board.parse_fen_castling(castling),
                false,
            )
        }
    }

    // -----------------------------------------------------------------------
    // Public FEN API
    // -----------------------------------------------------------------------

    pub fn set_fen(&mut self, fen: &str) -> bool {
        self.set_fen_internal(fen, false)
    }

    pub fn set_xfen(&mut self, xfen: &str) -> bool {
        let prev_960 = self.chess960_;
        self.chess960_ = true;
        let ok = self.set_fen_common(
            xfen,
            |board, castling| board.parse_xfen_castling(castling),
            false,
        );
        if !ok {
            self.chess960_ = prev_960;
        }
        ok
    }

    pub fn set_epd(&mut self, epd: &str) -> bool {
        let parts: Vec<&str> = epd.split_whitespace().collect();
        if parts.len() < 4 {
            return false;
        }

        let mut hm = 0i32;
        let mut fm = 1i32;

        // look for "hmvc" token
        if let Some(pos) = parts.iter().position(|&p| p == "hmvc") {
            if pos + 1 < parts.len() {
                if let Some(v) = Self::parse_int(parts[pos + 1]) {
                    hm = v;
                }
            } else {
                return false;
            }
        }

        // look for "fmvn" token
        if let Some(pos) = parts.iter().position(|&p| p == "fmvn") {
            if pos + 1 < parts.len() {
                if let Some(v) = Self::parse_int(parts[pos + 1]) {
                    if v > 0 {
                        fm = v;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        let fen = format!(
            "{} {} {} {} {} {}",
            parts[0], parts[1], parts[2], parts[3], hm, fm
        );
        self.set_fen(&fen)
    }

    pub fn set_960(&mut self, is_960: bool) {
        self.chess960_ = is_960;
        if !self.original_fen_.is_empty() {
            let fen = self.original_fen_.clone();
            let _ = self.set_fen(&fen);
        }
    }

    // -----------------------------------------------------------------------
    // FEN output
    // -----------------------------------------------------------------------

    fn append_fen_piece_placement(&self, out: &mut String) {
        for rank in (0i32..8).rev() {
            let mut free_space = 0u32;
            for file in 0i32..8 {
                let sq = Square((rank * 8 + file) as u8);
                let piece = self.at(sq);
                if piece != Piece::NONE {
                    if free_space > 0 {
                        out.push_str(&free_space.to_string());
                        free_space = 0;
                    }
                    out.push(piece.as_char());
                } else {
                    free_space += 1;
                }
            }
            if free_space > 0 {
                out.push_str(&free_space.to_string());
            }
            if rank > 0 {
                out.push('/');
            }
        }
    }

    fn castling_file_token(&self, color: Color, file: File) -> char {
        let c = (b'a' + file.as_u8()) as char;
        if color == Color::White {
            c.to_ascii_uppercase()
        } else {
            c
        }
    }

    pub fn get_castle_string(&self) -> String {
        if self.chess960_ {
            let mut s = String::new();
            for &color in &[Color::White, Color::Black] {
                for &is_king_side in &[true, false] {
                    if self.cr_.has(color, is_king_side) {
                        let f = self.cr_.get_rook_file(color, is_king_side);
                        s.push(self.castling_file_token(color, f));
                    }
                }
            }
            return s;
        }

        let mut s = String::new();
        if self.cr_.has(Color::White, true) {
            s.push('K');
        }
        if self.cr_.has(Color::White, false) {
            s.push('Q');
        }
        if self.cr_.has(Color::Black, true) {
            s.push('k');
        }
        if self.cr_.has(Color::Black, false) {
            s.push('q');
        }
        s
    }

    fn append_xfen_token(&self, out: &mut String, color: Color, is_king_side: bool) {
        if !self.cr_.has(color, is_king_side) {
            return;
        }
        let rook_file = self.cr_.get_rook_file(color, is_king_side);
        let outer_file = self.outer_rook_file(color, is_king_side);

        if outer_file != File::NoFile && rook_file == outer_file {
            let t = if is_king_side { 'K' } else { 'Q' };
            out.push(if color == Color::White {
                t
            } else {
                t.to_ascii_lowercase()
            });
            return;
        }
        out.push(self.castling_file_token(color, rook_file));
    }

    pub fn get_fen(&self, move_counters: bool) -> String {
        let mut s = String::with_capacity(100);
        self.append_fen_piece_placement(&mut s);
        s.push(' ');
        s.push(if self.stm_ == Color::White { 'w' } else { 'b' });
        s.push(' ');
        if self.cr_.is_empty() {
            s.push('-');
        } else {
            s.push_str(&self.get_castle_string());
        }
        s.push(' ');
        if self.ep_sq_ == Square::NO_SQ {
            s.push('-');
        } else {
            s.push_str(&self.ep_sq_.to_string());
        }
        if move_counters {
            s.push(' ');
            s.push_str(&self.half_move_clock().to_string());
            s.push(' ');
            s.push_str(&self.full_move_number().to_string());
        }
        s
    }

    pub fn get_xfen(&self, move_counters: bool) -> String {
        let mut s = String::with_capacity(100);
        self.append_fen_piece_placement(&mut s);
        s.push(' ');
        s.push(if self.stm_ == Color::White { 'w' } else { 'b' });
        s.push(' ');
        if self.cr_.is_empty() {
            s.push('-');
        } else {
            self.append_xfen_token(&mut s, Color::White, true);
            self.append_xfen_token(&mut s, Color::White, false);
            self.append_xfen_token(&mut s, Color::Black, true);
            self.append_xfen_token(&mut s, Color::Black, false);
        }
        s.push(' ');
        if self.ep_sq_ == Square::NO_SQ {
            s.push('-');
        } else {
            s.push_str(&self.ep_sq_.to_string());
        }
        if move_counters {
            s.push(' ');
            s.push_str(&self.half_move_clock().to_string());
            s.push(' ');
            s.push_str(&self.full_move_number().to_string());
        }
        s
    }

    pub fn get_epd(&self) -> String {
        let mut s = self.get_fen(false);
        s.push_str(" hmvc ");
        s.push_str(&self.half_move_clock().to_string());
        s.push(';');
        s.push_str(" fmvn ");
        s.push_str(&self.full_move_number().to_string());
        s.push(';');
        s
    }

    // -----------------------------------------------------------------------
    // Board accessors
    // -----------------------------------------------------------------------

    #[inline]
    pub fn at(&self, sq: Square) -> Piece {
        self.board_[sq.index()]
    }

    #[inline]
    pub fn at_type(&self, sq: Square) -> PieceType {
        self.board_[sq.index()].piece_type()
    }

    #[inline]
    pub fn pieces_pt_color(&self, pt: PieceType, color: Color) -> Bitboard {
        self.pieces_bb_[pt.as_u8() as usize] & self.occ_bb_[color.index()]
    }

    #[inline]
    pub fn pieces_pt(&self, pt: PieceType) -> Bitboard {
        self.pieces_bb_[pt.as_u8() as usize]
    }

    #[inline]
    pub fn pieces_two(&self, pt1: PieceType, pt2: PieceType) -> Bitboard {
        self.pieces_bb_[pt1.as_u8() as usize] | self.pieces_bb_[pt2.as_u8() as usize]
    }

    #[inline]
    pub fn us(&self, color: Color) -> Bitboard {
        self.occ_bb_[color.index()]
    }

    #[inline]
    pub fn them(&self, color: Color) -> Bitboard {
        self.occ_bb_[(!color).index()]
    }

    #[inline]
    pub fn occ(&self) -> Bitboard {
        self.occ_bb_[0] | self.occ_bb_[1]
    }

    #[inline]
    pub fn king_sq(&self, color: Color) -> Square {
        let bb = self.pieces_pt_color(PieceType::KING, color);
        if bb.0 == 0 {
            return Square::NO_SQ;
        }
        Square(bb.lsb())
    }

    #[inline]
    pub fn side_to_move(&self) -> Color {
        self.stm_
    }

    #[inline]
    pub fn enpassant_sq(&self) -> Square {
        self.ep_sq_
    }

    #[inline]
    pub fn castling_rights(&self) -> &CastlingRights {
        &self.cr_
    }

    #[inline]
    pub fn half_move_clock(&self) -> u32 {
        self.hfm_ as u32
    }

    #[inline]
    pub fn full_move_number(&self) -> u32 {
        1 + self.plies_ as u32 / 2
    }

    #[inline]
    pub fn hash(&self) -> u64 {
        self.key_
    }

    #[inline]
    pub fn chess960(&self) -> bool {
        self.chess960_
    }

    #[inline]
    pub fn get_castling_path(&self, c: Color, is_king_side: bool) -> Bitboard {
        self.castling_path[c.index()][is_king_side as usize]
    }

    pub fn is_capture(&self, m: Move) -> bool {
        (self.at(m.to()) != Piece::NONE && m.type_of() != Move::CASTLING)
            || m.type_of() == Move::ENPASSANT
    }

    // -----------------------------------------------------------------------
    // make_move
    // -----------------------------------------------------------------------

    /// Make a move. `exact` = true means validate ep squares strictly.
    pub fn make_move(&mut self, m: Move, exact: bool) {
        let capture = self.at(m.to()) != Piece::NONE && m.type_of() != Move::CASTLING;
        let captured = self.at(m.to());
        let pt = self.at_type(m.from());

        self.prev_states_.push(State {
            hash: self.key_,
            castling: self.cr_,
            enpassant: self.ep_sq_,
            half_moves: self.hfm_,
            captured_piece: captured,
        });

        self.hfm_ += 1;
        self.plies_ += 1;

        if self.ep_sq_ != Square::NO_SQ {
            self.key_ ^= Zobrist::enpassant(self.ep_sq_.file());
        }
        self.ep_sq_ = Square::NO_SQ;

        if capture {
            self.remove_piece(captured, m.to());
            self.hfm_ = 0;
            self.key_ ^= Zobrist::piece(captured, m.to());

            // Remove castling rights if a rook is captured
            if captured.piece_type() == PieceType::ROOK
                && Rank::back_rank(m.to().rank(), !self.stm_)
            {
                let king_sq = self.king_sq(!self.stm_);
                let is_king_side = CastlingRights::closest_side_sq(m.to(), king_sq);
                if self.cr_.get_rook_file(!self.stm_, is_king_side) == m.to().file() {
                    let idx = self.cr_.clear_one(!self.stm_, is_king_side);
                    self.key_ ^= Zobrist::castling_index(idx);
                }
            }
        }

        if pt == PieceType::KING && self.cr_.has_color(self.stm_) {
            self.key_ ^= Zobrist::castling(self.cr_.hash_index());
            self.cr_.clear_color(self.stm_);
            self.key_ ^= Zobrist::castling(self.cr_.hash_index());
        } else if pt == PieceType::ROOK && Square::back_rank(m.from(), self.stm_) {
            let king_sq = self.king_sq(self.stm_);
            let is_king_side = CastlingRights::closest_side_sq(m.from(), king_sq);
            if self.cr_.get_rook_file(self.stm_, is_king_side) == m.from().file() {
                let idx = self.cr_.clear_one(self.stm_, is_king_side);
                self.key_ ^= Zobrist::castling_index(idx);
            }
        } else if pt == PieceType::PAWN {
            self.hfm_ = 0;
            // Double push
            if Square::value_distance(m.to(), m.from()) == 16 {
                let ep_mask = attacks::pawn(self.stm_, m.to().ep_square());
                if (ep_mask & self.pieces_pt_color(PieceType::PAWN, !self.stm_)).0 != 0 {
                    let mut found: i32 = -1;

                    if exact {
                        let piece = self.at(m.from());
                        found = 0;

                        // Temporarily move the pawn
                        self.remove_piece(piece, m.from());
                        self.place_piece(piece, m.to());
                        self.stm_ = !self.stm_;

                        let ep_sq = m.to().ep_square();
                        let valid = movegen::is_ep_square_valid(self, ep_sq);
                        if valid {
                            found = 1;
                        }

                        // Undo
                        self.stm_ = !self.stm_;
                        self.remove_piece(piece, m.to());
                        self.place_piece(piece, m.from());
                    }

                    if found != 0 {
                        debug_assert!(self.at(m.to().ep_square()) == Piece::NONE);
                        self.ep_sq_ = m.to().ep_square();
                        self.key_ ^= Zobrist::enpassant(m.to().ep_square().file());
                    }
                }
            }
        }

        if m.type_of() == Move::CASTLING {
            let is_king_side = m.to() > m.from();
            let rook_to = Square::castling_rook_square(is_king_side, self.stm_);
            let king_to = Square::castling_king_square(is_king_side, self.stm_);

            let king = self.at(m.from());
            let rook = self.at(m.to());

            self.remove_piece(king, m.from());
            self.remove_piece(rook, m.to());
            self.place_piece(king, king_to);
            self.place_piece(rook, rook_to);

            self.key_ ^= Zobrist::piece(king, m.from()) ^ Zobrist::piece(king, king_to);
            self.key_ ^= Zobrist::piece(rook, m.to()) ^ Zobrist::piece(rook, rook_to);
        } else if m.type_of() == Move::PROMOTION {
            let piece_pawn = Piece::new(PieceType::PAWN, self.stm_);
            let piece_prom = Piece::new(m.promotion_type(), self.stm_);

            self.remove_piece(piece_pawn, m.from());
            self.place_piece(piece_prom, m.to());

            self.key_ ^= Zobrist::piece(piece_pawn, m.from()) ^ Zobrist::piece(piece_prom, m.to());
        } else {
            debug_assert!(self.at(m.from()) != Piece::NONE);
            debug_assert!(self.at(m.to()) == Piece::NONE);

            let piece = self.at(m.from());
            self.remove_piece(piece, m.from());
            self.place_piece(piece, m.to());

            self.key_ ^= Zobrist::piece(piece, m.from()) ^ Zobrist::piece(piece, m.to());
        }

        if m.type_of() == Move::ENPASSANT {
            let piece = Piece::new(PieceType::PAWN, !self.stm_);
            self.remove_piece(piece, m.to().ep_square());
            self.key_ ^= Zobrist::piece(piece, m.to().ep_square());
        }

        self.key_ ^= Zobrist::side_to_move();
        self.stm_ = !self.stm_;
    }

    // -----------------------------------------------------------------------
    // unmake_move
    // -----------------------------------------------------------------------

    pub fn unmake_move(&mut self, m: Move) {
        let prev = self
            .prev_states_
            .pop()
            .expect("unmake_move: no state to pop");

        self.ep_sq_ = prev.enpassant;
        self.cr_ = prev.castling;
        self.hfm_ = prev.half_moves;
        self.stm_ = !self.stm_;
        self.plies_ -= 1;

        if m.type_of() == Move::CASTLING {
            let is_king_side = m.to() > m.from();
            let rook_from_sq = if is_king_side {
                Square::new(File::FileF, m.from().rank())
            } else {
                Square::new(File::FileD, m.from().rank())
            };
            let king_to_sq = if is_king_side {
                Square::new(File::FileG, m.from().rank())
            } else {
                Square::new(File::FileC, m.from().rank())
            };

            let rook = self.at(rook_from_sq);
            let king = self.at(king_to_sq);

            self.remove_piece(rook, rook_from_sq);
            self.remove_piece(king, king_to_sq);
            self.place_piece(king, m.from());
            self.place_piece(rook, m.to());
        } else if m.type_of() == Move::PROMOTION {
            let pawn = Piece::new(PieceType::PAWN, self.stm_);
            let piece = self.at(m.to());

            self.remove_piece(piece, m.to());
            self.place_piece(pawn, m.from());

            if prev.captured_piece != Piece::NONE {
                self.place_piece(prev.captured_piece, m.to());
            }
        } else {
            debug_assert!(self.at(m.to()) != Piece::NONE);
            debug_assert!(self.at(m.from()) == Piece::NONE);

            let piece = self.at(m.to());
            self.remove_piece(piece, m.to());
            self.place_piece(piece, m.from());

            if m.type_of() == Move::ENPASSANT {
                let pawn = Piece::new(PieceType::PAWN, !self.stm_);
                let pawn_to = Square(self.ep_sq_.0 ^ 8);
                self.place_piece(pawn, pawn_to);
            } else if prev.captured_piece != Piece::NONE {
                self.place_piece(prev.captured_piece, m.to());
            }
        }

        self.key_ = prev.hash;
    }

    // -----------------------------------------------------------------------
    // make_null_move / unmake_null_move
    // -----------------------------------------------------------------------

    pub fn make_null_move(&mut self) {
        self.prev_states_.push(State {
            hash: self.key_,
            castling: self.cr_,
            enpassant: self.ep_sq_,
            half_moves: self.hfm_,
            captured_piece: Piece::NONE,
        });

        self.key_ ^= Zobrist::side_to_move();
        if self.ep_sq_ != Square::NO_SQ {
            self.key_ ^= Zobrist::enpassant(self.ep_sq_.file());
        }
        self.ep_sq_ = Square::NO_SQ;
        self.stm_ = !self.stm_;
        self.plies_ += 1;
    }

    pub fn unmake_null_move(&mut self) {
        let prev = self.prev_states_.pop().expect("unmake_null_move: no state");
        self.ep_sq_ = prev.enpassant;
        self.cr_ = prev.castling;
        self.hfm_ = prev.half_moves;
        self.key_ = prev.hash;
        self.plies_ -= 1;
        self.stm_ = !self.stm_;
    }

    // -----------------------------------------------------------------------
    // Zobrist (recompute from scratch)
    // -----------------------------------------------------------------------

    pub fn zobrist(&self) -> u64 {
        let mut hash_key = 0u64;
        let mut pieces = self.occ();
        while pieces.0 != 0 {
            let sq = Square(pieces.pop());
            hash_key ^= Zobrist::piece(self.at(sq), sq);
        }
        let ep_hash = if self.ep_sq_ != Square::NO_SQ {
            Zobrist::enpassant(self.ep_sq_.file())
        } else {
            0
        };
        let stm_hash = if self.stm_ == Color::White {
            Zobrist::side_to_move()
        } else {
            0
        };
        let castling_hash = Zobrist::castling(self.cr_.hash_index());
        hash_key ^ ep_hash ^ stm_hash ^ castling_hash
    }

    // -----------------------------------------------------------------------
    // zobristAfter (without actually making the move)
    // -----------------------------------------------------------------------

    pub fn zobrist_after(&self, m: Move, exact: bool) -> u64 {
        let mut key = self.key_;
        key ^= Zobrist::side_to_move();
        if exact && self.ep_sq_ != Square::NO_SQ {
            key ^= Zobrist::enpassant(self.ep_sq_.file());
        }

        if m.raw() == Move::NULL_MOVE {
            return key;
        }

        let capture = self.at(m.to()) != Piece::NONE && m.type_of() != Move::CASTLING;
        let captured = self.at(m.to());
        let pt = self.at_type(m.from());

        if capture {
            key ^= Zobrist::piece(captured, m.to());
            if exact
                && captured.piece_type() == PieceType::ROOK
                && Rank::back_rank(m.to().rank(), !self.stm_)
            {
                let king_sq = self.king_sq(!self.stm_);
                let is_king_side = CastlingRights::closest_side_sq(m.to(), king_sq);
                if self.cr_.get_rook_file(!self.stm_, is_king_side) == m.to().file() {
                    key ^= Zobrist::castling_index(
                        (!self.stm_).index() * 2 + (!is_king_side) as usize,
                    );
                }
            }
        }

        if exact {
            if pt == PieceType::KING && self.cr_.has_color(self.stm_) {
                let old_idx = self.cr_.hash_index();
                let new_idx = old_idx & (if self.stm_ == Color::Black { 3 } else { 12 });
                key ^= Zobrist::castling(old_idx);
                key ^= Zobrist::castling(new_idx);
            } else if pt == PieceType::ROOK && Square::back_rank(m.from(), self.stm_) {
                let king_sq = self.king_sq(self.stm_);
                let is_king_side = CastlingRights::closest_side_sq(m.from(), king_sq);
                if self.cr_.get_rook_file(self.stm_, is_king_side) == m.from().file() {
                    key ^=
                        Zobrist::castling_index(self.stm_.index() * 2 + (!is_king_side) as usize);
                }
            } else if pt == PieceType::PAWN {
                if Square::value_distance(m.to(), m.from()) == 16 {
                    let ep_mask = attacks::pawn(self.stm_, m.to().ep_square());
                    if (ep_mask & self.pieces_pt_color(PieceType::PAWN, !self.stm_)).0 != 0 {
                        key ^= Zobrist::enpassant(m.to().ep_square().file());
                    }
                }
            }
        }

        if m.type_of() != Move::CASTLING && m.type_of() != Move::PROMOTION {
            let piece = self.at(m.from());
            key ^= Zobrist::piece(piece, m.from()) ^ Zobrist::piece(piece, m.to());
        } else if exact {
            if m.type_of() == Move::CASTLING {
                let is_king_side = m.to() > m.from();
                let rook_to = Square::castling_rook_square(is_king_side, self.stm_);
                let king_to = Square::castling_king_square(is_king_side, self.stm_);
                let king = self.at(m.from());
                let rook = self.at(m.to());
                key ^= Zobrist::piece(king, m.from()) ^ Zobrist::piece(king, king_to);
                key ^= Zobrist::piece(rook, m.to()) ^ Zobrist::piece(rook, rook_to);
            } else {
                let piece_pawn = Piece::new(PieceType::PAWN, self.stm_);
                let piece_prom = Piece::new(m.promotion_type(), self.stm_);
                key ^= Zobrist::piece(piece_pawn, m.from()) ^ Zobrist::piece(piece_prom, m.to());
            }
        }

        if exact && m.type_of() == Move::ENPASSANT {
            let piece = Piece::new(PieceType::PAWN, !self.stm_);
            key ^= Zobrist::piece(piece, m.to().ep_square());
        }

        key
    }

    // -----------------------------------------------------------------------
    // Game state queries
    // -----------------------------------------------------------------------

    pub fn is_attacked(&self, square: Square, color: Color) -> bool {
        if (attacks::pawn(!color, square) & self.pieces_pt_color(PieceType::PAWN, color)).0 != 0 {
            return true;
        }
        if (attacks::knight(square) & self.pieces_pt_color(PieceType::KNIGHT, color)).0 != 0 {
            return true;
        }
        if (attacks::king(square) & self.pieces_pt_color(PieceType::KING, color)).0 != 0 {
            return true;
        }
        let bq = self.pieces_two(PieceType::BISHOP, PieceType::QUEEN) & self.us(color);
        if (attacks::bishop(square, self.occ()) & bq).0 != 0 {
            return true;
        }
        let rq = self.pieces_two(PieceType::ROOK, PieceType::QUEEN) & self.us(color);
        if (attacks::rook(square, self.occ()) & rq).0 != 0 {
            return true;
        }
        false
    }

    pub fn in_check(&self) -> bool {
        self.is_attacked(self.king_sq(self.stm_), !self.stm_)
    }

    pub fn has_non_pawn_material(&self, color: Color) -> bool {
        let pawn_king =
            (self.pieces_pt(PieceType::PAWN) | self.pieces_pt(PieceType::KING)) & self.us(color);
        (self.us(color) ^ pawn_king).0 != 0
    }

    pub fn is_repetition(&self, count: u32) -> bool {
        let mut c = 0u32;
        let size = self.prev_states_.len() as i32;
        let mut i = size - 2;
        while i >= 0 && i >= size - self.hfm_ as i32 - 1 {
            if self.prev_states_[i as usize].hash == self.key_ {
                c += 1;
                if c == count {
                    return true;
                }
            }
            i -= 2;
        }
        false
    }

    pub fn is_half_move_draw(&self) -> bool {
        self.hfm_ >= 100
    }

    pub fn is_insufficient_material(&self) -> bool {
        let count = self.occ().count();
        if count == 2 {
            return true;
        }
        if count == 3 {
            if self.pieces_pt_color(PieceType::BISHOP, Color::White).0 != 0
                || self.pieces_pt_color(PieceType::BISHOP, Color::Black).0 != 0
            {
                return true;
            }
            if self.pieces_pt_color(PieceType::KNIGHT, Color::White).0 != 0
                || self.pieces_pt_color(PieceType::KNIGHT, Color::Black).0 != 0
            {
                return true;
            }
        }
        if count == 4 {
            let wb = self.pieces_pt_color(PieceType::BISHOP, Color::White);
            let bb = self.pieces_pt_color(PieceType::BISHOP, Color::Black);
            if wb.0 != 0 && bb.0 != 0 {
                if Square::same_color(Square(wb.lsb()), Square(bb.lsb())) {
                    return true;
                }
            }
            if wb.count() == 2 {
                let lsb = Square(wb.lsb());
                let msb = Square(wb.msb());
                if Square::same_color(lsb, msb) {
                    return true;
                }
            } else if bb.count() == 2 {
                let lsb = Square(bb.lsb());
                let msb = Square(bb.msb());
                if Square::same_color(lsb, msb) {
                    return true;
                }
            }
        }
        false
    }

    pub fn gives_check(&self, m: Move) -> CheckType {
        let from = m.from();
        let to = m.to();
        let ksq = self.king_sq(!self.stm_);
        let to_bb = Bitboard::from_square(to);
        let pt = self.at(from).piece_type();

        let from_king = match pt {
            PieceType::PAWN => attacks::pawn(!self.stm_, ksq),
            PieceType::KNIGHT => attacks::knight(ksq),
            PieceType::BISHOP => attacks::bishop(ksq, self.occ()),
            PieceType::ROOK => attacks::rook(ksq, self.occ()),
            PieceType::QUEEN => attacks::rook(ksq, self.occ()) | attacks::bishop(ksq, self.occ()),
            _ => Bitboard(0),
        };

        if (from_king & to_bb).0 != 0 {
            return CheckType::DirectCheck;
        }

        // Discovery check
        let from_bb = Bitboard::from_square(from);
        let oc = self.occ() ^ from_bb;
        let us_occ = self.us(self.stm_);
        let bishop_sniper = attacks::bishop(ksq, oc)
            & self.pieces_two(PieceType::BISHOP, PieceType::QUEEN)
            & us_occ;
        let rook_sniper =
            attacks::rook(ksq, oc) & self.pieces_two(PieceType::ROOK, PieceType::QUEEN) & us_occ;
        let mut sniper = bishop_sniper | rook_sniper;

        while sniper.0 != 0 {
            let sq = Square(sniper.pop());
            return if (movegen::between(ksq, sq) & to_bb).0 == 0 || m.type_of() == Move::CASTLING {
                CheckType::DiscoveryCheck
            } else {
                CheckType::NoCheck
            };
        }

        match m.type_of() {
            Move::NORMAL => CheckType::NoCheck,
            Move::PROMOTION => {
                let atk = match m.promotion_type() {
                    PieceType::KNIGHT => attacks::knight(to),
                    PieceType::BISHOP => attacks::bishop(to, oc),
                    PieceType::ROOK => attacks::rook(to, oc),
                    PieceType::QUEEN => attacks::rook(to, oc) | attacks::bishop(to, oc),
                    _ => Bitboard(0),
                };
                if (atk & self.pieces_pt_color(PieceType::KING, !self.stm_)).0 != 0 {
                    CheckType::DirectCheck
                } else {
                    CheckType::NoCheck
                }
            }
            Move::ENPASSANT => {
                let cap_sq = Square::new(to.file(), from.rank());
                let new_oc = (oc ^ Bitboard::from_square(cap_sq)) | to_bb;
                let bishop_sn = attacks::bishop(ksq, new_oc)
                    & self.pieces_two(PieceType::BISHOP, PieceType::QUEEN)
                    & us_occ;
                let rook_sn = attacks::rook(ksq, new_oc)
                    & self.pieces_two(PieceType::ROOK, PieceType::QUEEN)
                    & us_occ;
                if (bishop_sn | rook_sn).0 != 0 {
                    CheckType::DiscoveryCheck
                } else {
                    CheckType::NoCheck
                }
            }
            Move::CASTLING => {
                let rook_to = Square::castling_rook_square(to > from, self.stm_);
                if (attacks::rook(ksq, self.occ()) & Bitboard::from_square(rook_to)).0 != 0 {
                    CheckType::DiscoveryCheck
                } else {
                    CheckType::NoCheck
                }
            }
            _ => CheckType::NoCheck,
        }
    }

    pub fn is_game_over(&self) -> (GameResultReason, GameResult) {
        if self.is_half_move_draw() {
            return self.get_half_move_draw_type();
        }
        if self.is_insufficient_material() {
            return (GameResultReason::InsufficientMaterial, GameResult::Draw);
        }
        if self.is_repetition(2) {
            return (GameResultReason::ThreefoldRepetition, GameResult::Draw);
        }

        use crate::variants::chessport::movegen::{legalmoves, MoveGenType, PieceGenType};
        let mut ml = crate::variants::chessport::movelist::Movelist::new();
        legalmoves(&mut ml, self, MoveGenType::All, PieceGenType::ALL);

        if ml.is_empty() {
            if self.in_check() {
                return (GameResultReason::Checkmate, GameResult::Lose);
            }
            return (GameResultReason::Stalemate, GameResult::Draw);
        }

        (GameResultReason::None, GameResult::None)
    }

    pub fn get_half_move_draw_type(&self) -> (GameResultReason, GameResult) {
        use crate::variants::chessport::movegen::{legalmoves, MoveGenType, PieceGenType};
        let mut ml = crate::variants::chessport::movelist::Movelist::new();
        legalmoves(&mut ml, self, MoveGenType::All, PieceGenType::ALL);

        if ml.is_empty() && self.in_check() {
            return (GameResultReason::Checkmate, GameResult::Lose);
        }
        (GameResultReason::FiftyMoveRule, GameResult::Draw)
    }
}

// ---------------------------------------------------------------------------
// Default
// ---------------------------------------------------------------------------

impl Default for Board {
    fn default() -> Self {
        Board::new()
    }
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

impl std::fmt::Display for Board {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for i in (0i32..64).rev().step_by(8) {
            for j in (0i32..8).rev() {
                write!(f, " {}", self.board_[(i - j) as usize].as_char())?;
            }
            writeln!(f)?;
        }
        writeln!(f)?;
        writeln!(f, "Side to move: {}", self.stm_)?;
        writeln!(f, "Castling rights: {}", self.get_castle_string())?;
        writeln!(f, "Halfmoves: {}", self.half_move_clock())?;
        writeln!(f, "Fullmoves: {}", self.full_move_number())?;
        writeln!(f, "EP: {}", self.ep_sq_.0)?;
        writeln!(f, "Hash: {}", self.key_)?;
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// PartialEq
// ---------------------------------------------------------------------------

impl PartialEq for Board {
    fn eq(&self, other: &Self) -> bool {
        self.pieces_bb_ == other.pieces_bb_
            && self.occ_bb_ == other.occ_bb_
            && self.board_ == other.board_
            && self.key_ == other.key_
            && self.cr_ == other.cr_
            && self.plies_ == other.plies_
            && self.stm_ == other.stm_
            && self.ep_sq_ == other.ep_sq_
            && self.hfm_ == other.hfm_
            && self.chess960_ == other.chess960_
            && self.castling_path == other.castling_path
    }
}

impl Eq for Board {}

// ---------------------------------------------------------------------------
// Compact (PackedBoard encode/decode)
// ---------------------------------------------------------------------------

pub struct Compact;

impl Compact {
    pub fn encode(board: &Board) -> PackedBoard {
        Self::encode_state(board)
    }

    pub fn encode_fen(fen: &str, chess960: bool) -> PackedBoard {
        if chess960 {
            return Self::encode_state(&Board::from_fen(fen));
        }

        let mut packed = [0u8; 24];
        let parts: Vec<&str> = fen.trim_start().splitn(5, ' ').collect();
        let position = parts.get(0).copied().unwrap_or("");
        let move_right = parts.get(1).copied().unwrap_or("w");
        let castling = parts.get(2).copied().unwrap_or("-");
        let en_passant = parts.get(3).copied().unwrap_or("-");

        let ep = if en_passant == "-" {
            Square::NO_SQ
        } else {
            Square::from_str(en_passant)
        };
        let stm = if move_right == "w" {
            Color::White
        } else {
            Color::Black
        };

        let mut cr = CastlingRights::new();
        for c in castling.chars() {
            if c == '-' {
                break;
            }
            match c {
                'K' => cr.set(Color::White, true, File::FileH),
                'Q' => cr.set(Color::White, false, File::FileA),
                'k' => cr.set(Color::Black, true, File::FileH),
                'q' => cr.set(Color::Black, false, File::FileA),
                _ => {}
            }
        }

        // Parse position; FEN rows go rank8 first
        let mut offset = 8 * 2usize;
        let mut square = 0usize;
        let mut occ = Bitboard(0);

        let rows: Vec<&str> = position.split('/').collect();
        for row in rows.iter().rev() {
            for c in row.chars() {
                if c.is_ascii_digit() {
                    square += (c as u8 - b'0') as usize;
                } else {
                    let p = Piece::from_char(c);
                    let shift = if offset % 2 == 0 { 4 } else { 0 };
                    packed[offset / 2] |=
                        Self::convert_meaning(&cr, stm, ep, Square(square as u8), p) << shift;
                    offset += 1;
                    occ.set(square as u8);
                    square += 1;
                }
            }
        }

        // Write occupancy bitboard (big-endian)
        let bits = occ.0.to_be_bytes();
        packed[..8].copy_from_slice(&bits);
        packed
    }

    pub fn decode(compressed: &PackedBoard, chess960: bool) -> Board {
        let mut board = Board::empty();
        board.chess960_ = chess960;
        Self::decode_into(&mut board, compressed);
        board
    }

    fn encode_state(board: &Board) -> PackedBoard {
        let mut packed = [0u8; 24];

        // Occupancy bitboard (big-endian)
        let occ_bits = board.occ().0.to_be_bytes();
        packed[..8].copy_from_slice(&occ_bits);

        let mut offset = 8 * 2usize;
        let mut occ = board.occ();

        while occ.0 != 0 {
            let sq = Square(occ.pop());
            let shift = if offset % 2 == 0 { 4 } else { 0 };
            let meaning =
                Self::convert_meaning(&board.cr_, board.stm_, board.ep_sq_, sq, board.at(sq));
            packed[offset / 2] |= meaning << shift;
            offset += 1;
        }

        packed
    }

    fn decode_into(board: &mut Board, compressed: &PackedBoard) {
        let mut occupied = Bitboard(0);
        for i in 0..8 {
            occupied = occupied | Bitboard((compressed[i] as u64) << (56 - i * 8));
        }

        board.hfm_ = 0;
        board.plies_ = 0;
        board.stm_ = Color::White;
        board.cr_.clear_all();
        board.prev_states_.clear();
        board.original_fen_.clear();
        board.occ_bb_ = [Bitboard(0); 2];
        board.pieces_bb_ = [Bitboard(0); 6];
        board.board_ = [Piece::NONE; 64];

        let mut white_castle = [File::NoFile; 2];
        let mut black_castle = [File::NoFile; 2];
        let mut white_castle_idx = 0usize;
        let mut black_castle_idx = 0usize;

        let mut offset = 16usize;

        let mut occ2 = occupied;
        while occ2.0 != 0 {
            let sq = Square(occ2.pop());
            let nibble = (compressed[offset / 2] >> if offset % 2 == 0 { 4 } else { 0 }) & 0b1111;
            let piece = Self::convert_piece_from_nibble(nibble);

            if piece != Piece::NONE {
                board.place_piece(piece, sq);
                offset += 1;
                continue;
            }

            match nibble {
                12 => {
                    board.ep_sq_ = sq.ep_square();
                    let color = if sq.rank() == Rank::Rank4 {
                        Color::White
                    } else {
                        Color::Black
                    };
                    board.place_piece(Piece::new(PieceType::PAWN, color), sq);
                }
                13 => {
                    if white_castle_idx < 2 {
                        white_castle[white_castle_idx] = sq.file();
                        white_castle_idx += 1;
                    }
                    board.place_piece(Piece::new(PieceType::ROOK, Color::White), sq);
                }
                14 => {
                    if black_castle_idx < 2 {
                        black_castle[black_castle_idx] = sq.file();
                        black_castle_idx += 1;
                    }
                    board.place_piece(Piece::new(PieceType::ROOK, Color::Black), sq);
                }
                15 => {
                    board.stm_ = Color::Black;
                    board.place_piece(Piece::new(PieceType::KING, Color::Black), sq);
                }
                _ => {}
            }
            offset += 1;
        }

        // Reapply castling rights
        for i in 0..2 {
            if white_castle[i] != File::NoFile {
                let king_sq = board.king_sq(Color::White);
                let file = white_castle[i];
                let is_king_side = CastlingRights::closest_side_file(file, king_sq.file());
                board.cr_.set(Color::White, is_king_side, file);
            }
            if black_castle[i] != File::NoFile {
                let king_sq = board.king_sq(Color::Black);
                let file = black_castle[i];
                let is_king_side = CastlingRights::closest_side_file(file, king_sq.file());
                board.cr_.set(Color::Black, is_king_side, file);
            }
        }

        if board.stm_ == Color::Black {
            board.plies_ += 1;
        }

        board.key_ = board.zobrist();
        board.castling_path = [[Bitboard(0); 2]; 2];
        board.recompute_castling_path();
    }

    /// Convert a piece to its nibble for packing.
    fn convert_meaning(
        cr: &CastlingRights,
        stm: Color,
        ep: Square,
        sq: Square,
        piece: Piece,
    ) -> u8 {
        let pt = piece.piece_type();
        // ep pawn
        if pt == PieceType::PAWN && ep != Square::NO_SQ {
            if Square(sq.0 ^ 8) == ep {
                return 12;
            }
        }
        // rook with castling
        if pt == PieceType::ROOK {
            if piece.color() == Color::White
                && Square::back_rank(sq, Color::White)
                && (cr.get_rook_file(Color::White, true) == sq.file()
                    || cr.get_rook_file(Color::White, false) == sq.file())
            {
                return 13;
            }
            if piece.color() == Color::Black
                && Square::back_rank(sq, Color::Black)
                && (cr.get_rook_file(Color::Black, true) == sq.file()
                    || cr.get_rook_file(Color::Black, false) == sq.file())
            {
                return 14;
            }
        }
        // black king + black to move
        if pt == PieceType::KING && piece.color() == Color::Black && stm == Color::Black {
            return 15;
        }

        piece.as_u8()
    }

    fn convert_piece_from_nibble(nibble: u8) -> Piece {
        if nibble >= 12 {
            Piece::NONE
        } else {
            Piece::from_u8(nibble)
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use crate::variants::chessport::movelist::Movelist;

    use super::*;
    use std::time::Instant;

    #[test]
    fn test_startpos_fen_roundtrip() {
        let board = Board::new();
        let fen = board.get_fen(true);
        assert_eq!(fen, STARTPOS);
    }

    #[test]
    fn test_zobrist_consistency() {
        let board = Board::new();
        assert_eq!(board.hash(), board.zobrist());
    }

    #[test]
    fn test_make_unmake() {
        let mut board = Board::new();
        let hash_before = board.hash();
        let fen_before = board.get_fen(true);

        // e2e4
        let m = Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_E4, PieceType::KNIGHT);
        board.make_move(m, false);
        assert_ne!(board.hash(), hash_before);

        board.unmake_move(m);
        assert_eq!(board.hash(), hash_before);
        assert_eq!(board.get_fen(true), fen_before);
    }

    #[test]
    fn test_full_move_number() {
        let board = Board::new();
        assert_eq!(board.full_move_number(), 1);
    }

    #[test]
    fn test_half_move_clock() {
        let board = Board::new();
        assert_eq!(board.half_move_clock(), 0);
    }

    #[test]
    fn test_side_to_move() {
        let board = Board::new();
        assert_eq!(board.side_to_move(), Color::White);
    }

    #[test]
    fn test_king_sq() {
        let board = Board::new();
        assert_eq!(board.king_sq(Color::White), Square::SQ_E1);
        assert_eq!(board.king_sq(Color::Black), Square::SQ_E8);
    }

    #[test]
    fn test_in_check_startpos() {
        let board = Board::new();
        assert!(!board.in_check());
    }

    #[test]
    fn test_fen_with_ep() {
        let fen = "rnbqkbnr/pppp1ppp/8/3Pp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 2";
        let board = Board::from_fen(fen);
        assert_eq!(board.enpassant_sq(), Square::SQ_E6);
        assert_eq!(board.hash(), board.zobrist());

        let fen_no_ep = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        let board2 = Board::from_fen(fen_no_ep);
        assert_eq!(board2.enpassant_sq(), Square::NO_SQ);
        assert_eq!(board2.hash(), board2.zobrist());
    }

    // -- Ported from tests/board.cpp --

    #[test]
    fn set_fen() {
        let mut board = Board::new();
        assert!(board.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - "));
        assert_eq!(
            board.get_fen(true),
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
        );
    }

    #[test]
    fn set_xfen() {
        let mut board = Board::new();
        assert!(board.set_xfen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
        assert!(board.chess960());
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::White, CastlingRights::KING_SIDE),
            File::FILE_H
        );
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::White, CastlingRights::QUEEN_SIDE),
            File::FILE_A
        );
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::Black, CastlingRights::KING_SIDE),
            File::FILE_H
        );
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::Black, CastlingRights::QUEEN_SIDE),
            File::FILE_A
        );
        assert_eq!(
            board.get_xfen(true),
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        );
    }

    #[test]
    fn xfen_lichess_example() {
        assert_eq!(
            Board::from_xfen("rnbnkqrb/pppppppp/8/8/8/8/PPPPPPPP/RNBNKQRB w KQkq - 0 1")
                .get_xfen(true),
            "rnbnkqrb/pppppppp/8/8/8/8/PPPPPPPP/RNBNKQRB w KQkq - 0 1"
        );
        assert_eq!(
            Board::from_xfen("rnb1k1r1/ppp1pp1p/3p2p1/6n1/P7/2N2B2/1PPPPP2/2BNK1RR b Gkq - 3 10")
                .get_xfen(true),
            "rnb1k1r1/ppp1pp1p/3p2p1/6n1/P7/2N2B2/1PPPPP2/2BNK1RR b Gkq - 3 10"
        );
    }

    #[test]
    fn xfen_castling_uses_outermost_rook() {
        let mut board = Board::new();
        assert!(board.set_xfen("4k3/8/8/8/8/8/8/4KR1R w K - 0 1"));
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::White, CastlingRights::KING_SIDE),
            File::FILE_H
        );
        assert_eq!(board.get_xfen(true), "4k3/8/8/8/8/8/8/4KR1R w K - 0 1");
    }

    #[test]
    fn xfen_castling_file_letter_selects_rook() {
        let mut board = Board::new();
        assert!(board.set_xfen("4k3/8/8/8/8/8/8/4KR1R w F - 0 1"));
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::White, CastlingRights::KING_SIDE),
            File::FILE_F
        );
        assert_eq!(board.get_xfen(true), "4k3/8/8/8/8/8/8/4KR1R w F - 0 1");
    }

    #[test]
    fn xfen_castling_file_letter_lowercase_for_black() {
        let mut board = Board::new();
        assert!(board.set_xfen("4kr1r/8/8/8/8/8/8/4K3 b f - 0 1"));
        assert_eq!(
            board
                .castling_rights()
                .get_rook_file(Color::Black, CastlingRights::KING_SIDE),
            File::FILE_F
        );
        assert_eq!(board.get_xfen(true), "4kr1r/8/8/8/8/8/8/4K3 b f - 0 1");
    }

    #[test]
    fn xfen_with_makemove() {
        let mut board =
            Board::from_fen("rnb1k1r1/ppp1pp1p/3p2p1/6n1/P7/2N2B1R/1PPPPP2/2BNK1R1 w Kkq - 2 10");
        let mv =
            crate::variants::chessport::uci::parse_san(&board, "Rhh1").expect("Rhh1 should parse");
        board.make_move(mv, false);
        assert_eq!(
            board.get_xfen(true),
            "rnb1k1r1/ppp1pp1p/3p2p1/6n1/P7/2N2B2/1PPPPP2/2BNK1RR b Gkq - 3 10"
        );
    }

    #[test]
    fn xfen_python_chess_wiki_example() {
        let xfen = "rn2k1r1/ppp1pp1p/3p2p1/5bn1/P7/2N2B2/1PPPPP2/2BNK1RR w Gkq - 4 11";
        let board = Board::from_xfen(xfen);
        let cr = board.castling_rights();
        assert!(board.chess960());
        assert_eq!(
            cr.get_rook_file(Color::White, CastlingRights::KING_SIDE),
            File::FILE_G
        );
        assert_eq!(
            cr.get_rook_file(Color::Black, CastlingRights::KING_SIDE),
            File::FILE_G
        );
        assert_eq!(
            cr.get_rook_file(Color::Black, CastlingRights::QUEEN_SIDE),
            File::FILE_A
        );
        assert!(!cr.has(Color::White, CastlingRights::QUEEN_SIDE));
        assert_eq!(board.get_xfen(true), xfen);
        assert!(cr.has_color(Color::White));
        assert!(cr.has_color(Color::Black));
        assert!(cr.has(Color::Black, CastlingRights::KING_SIDE));
        assert!(cr.has(Color::White, CastlingRights::KING_SIDE));
        assert!(cr.has(Color::Black, CastlingRights::QUEEN_SIDE));
    }

    #[test]
    fn xfen_chess960_position_284() {
        let xfen = "rkbqrbnn/pppppppp/8/8/8/8/PPPPPPPP/RKBQRBNN w KQkq - 0 1";
        let board = Board::from_xfen(xfen);
        let cr = board.castling_rights();
        assert_eq!(
            cr.get_rook_file(Color::White, CastlingRights::KING_SIDE),
            File::FILE_E
        );
        assert_eq!(
            cr.get_rook_file(Color::White, CastlingRights::QUEEN_SIDE),
            File::FILE_A
        );
        assert_eq!(
            cr.get_rook_file(Color::Black, CastlingRights::KING_SIDE),
            File::FILE_E
        );
        assert_eq!(
            cr.get_rook_file(Color::Black, CastlingRights::QUEEN_SIDE),
            File::FILE_A
        );
        assert_eq!(board.get_xfen(true), xfen);
    }

    #[test]
    fn board_make_move_unmake_move() {
        let mut board = Board::new();
        board.make_move(
            Move::make::<{ Move::NORMAL }>(Square::SQ_E2, Square::SQ_E4, PieceType::KNIGHT),
            false,
        );
        board.make_move(
            Move::make::<{ Move::NORMAL }>(Square::SQ_E7, Square::SQ_E5, PieceType::KNIGHT),
            false,
        );
        assert_eq!(board.at(Square::SQ_E4), Piece::WHITE_PAWN);
        assert_eq!(board.at(Square::SQ_E5), Piece::BLACK_PAWN);
        board.unmake_move(Move::make::<{ Move::NORMAL }>(
            Square::SQ_E7,
            Square::SQ_E5,
            PieceType::KNIGHT,
        ));
        board.unmake_move(Move::make::<{ Move::NORMAL }>(
            Square::SQ_E2,
            Square::SQ_E4,
            PieceType::KNIGHT,
        ));
        assert_eq!(board.at(Square::SQ_E2), Piece::WHITE_PAWN);
        assert_eq!(board.at(Square::SQ_E7), Piece::BLACK_PAWN);
        assert_eq!(board.zobrist(), Board::new().zobrist());
    }

    #[test]
    fn board_make_null_move() {
        let mut board = Board::new();
        board.make_null_move();
        assert_ne!(board.zobrist(), Board::new().zobrist());
        assert_eq!(board.side_to_move(), Color::Black);
        board.unmake_null_move();
        assert_eq!(board.zobrist(), Board::new().zobrist());
        assert_eq!(board.side_to_move(), Color::White);
    }

    #[test]
    fn board_has_non_pawn_material() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        assert!(!board.has_non_pawn_material(board.side_to_move()));
        assert!(!board.has_non_pawn_material(Color::White));
        assert!(board.has_non_pawn_material(Color::Black));
    }

    #[test]
    fn board_half_move_draw() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        assert!(!board.is_half_move_draw());
    }

    #[test]
    fn board_half_move_draw_true() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 100 1");
        assert!(board.is_half_move_draw());
        assert_eq!(
            board.get_half_move_draw_type().0,
            GameResultReason::FiftyMoveRule
        );
    }

    #[test]
    fn board_half_move_draw_true_and_checkmate() {
        let board = Board::from_fen("7k/8/5B1K/8/8/1B6/8/8 b - - 100 1");
        assert!(board.is_half_move_draw());
        assert_eq!(
            board.get_half_move_draw_type().0,
            GameResultReason::Checkmate
        );
    }

    #[test]
    fn board_fen_get_set() {
        let mut board = Board::new();
        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        assert_eq!(
            board.get_fen(true),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1"
        );

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 3 24");
        assert_eq!(
            board.get_fen(true),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 3 24"
        );

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w KQkq - 0 1");
        assert_eq!(
            board.get_fen(true),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1"
        );

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w KQkq e3 0 1");
        assert_eq!(
            board.get_fen(true),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1"
        );

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w KQkq e3 0 1");
        assert_eq!(
            board.get_fen(false),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - -"
        );

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 3 24");
        assert_eq!(
            board.get_fen(false),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - -"
        );

        board.set_fen("r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq -");
        assert_eq!(
            board.get_fen(true),
            "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1"
        );
    }

    #[test]
    fn board_epd_set_get() {
        let mut board = Board::new();
        board.set_epd(
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - hmvc 0; fmvn 9;",
        );
        assert_eq!(
            board.get_fen(true),
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9"
        );
        assert_eq!(
            board.get_epd(),
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - hmvc 0; fmvn 9;"
        );
    }

    #[test]
    fn board_epd_without_fmvn() {
        let mut board = Board::new();
        board.set_epd("r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - hmvc 0;");
        assert_eq!(
            board.get_fen(true),
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 1"
        );
    }

    #[test]
    fn board_epd_without_hmvc() {
        let mut board = Board::new();
        board.set_epd("r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - fmvn 9;");
        assert_eq!(
            board.get_fen(true),
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9"
        );
    }

    #[test]
    fn board_from_fen() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        assert_eq!(
            board.get_fen(true),
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1"
        );
    }

    #[test]
    fn board_from_fen_with_invalid_ep() {
        let board = Board::from_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
        assert_eq!(
            board.get_fen(true),
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"
        );
    }

    #[test]
    fn board_from_epd() {
        let mut board = Board::new();
        board.set_epd(
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - hmvc 0; fmvn 9;",
        );
        assert_eq!(
            board.get_fen(true),
            "r1bqkb1r/pp3pp1/2nppn2/7p/3NP1PP/2N5/PPP2P2/R1BQKBR1 w Qkq - 0 9"
        );
    }

    // -- Insufficient Material --

    #[test]
    fn insufficient_material_two_white_light_bishops() {
        let board = Board::from_fen("8/6k1/8/8/4B3/3B4/8/1K6 w - - 0 1");
        assert!(board.is_insufficient_material());
    }

    #[test]
    fn insufficient_material_two_black_light_bishops() {
        let board = Board::from_fen("8/6k1/8/8/4b3/3b4/K7/8 w - - 0 1");
        assert!(board.is_insufficient_material());
    }

    #[test]
    fn insufficient_material_white_bishop() {
        let board = Board::from_fen("8/7k/8/8/3B4/8/8/1K6 w - - 0 1");
        assert!(board.is_insufficient_material());
    }

    #[test]
    fn insufficient_material_black_bishop() {
        let board = Board::from_fen("8/7k/8/8/3b4/8/8/1K6 w - - 0 1");
        assert!(board.is_insufficient_material());
    }

    #[test]
    fn insufficient_material_white_knight() {
        let board = Board::from_fen("8/7k/8/8/3N4/8/8/1K6 w - - 0 1");
        assert!(board.is_insufficient_material());
    }

    #[test]
    fn insufficient_material_black_knight() {
        let board = Board::from_fen("8/7k/8/8/3n4/8/8/1K6 w - - 0 1");
        assert!(board.is_insufficient_material());
    }

    #[test]
    fn insufficient_material_white_light_and_dark_bishop() {
        let board = Board::from_fen("8/7k/8/8/3BB3/8/8/1K6 w - - 0 1");
        assert!(!board.is_insufficient_material());
    }

    // -- PackedBoard --

    #[test]
    fn packed_board_encode_decode() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
        assert_eq!(std::mem::size_of_val(&compressed), 24);
    }

    #[test]
    fn packed_board_same_zobrist_hash() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert_eq!(board.hash(), newboard.hash());
    }

    #[test]
    fn packed_board_ep_square() {
        let board = Board::from_fen("4k1n1/ppp1pppp/8/8/3pP3/8/PPPP1PPP/4K3 b - e3 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_white_to_move() {
        let board = Board::from_fen("4k1n1/ppp1p1pp/8/4Pp2/3p4/8/PPPP1PPP/4K3 w - f6 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_white_castling_bug() {
        let board = Board::from_fen("rnb1kbnR/pppp4/5q2/4pp2/8/8/PPPPPP1P/RNBQKBNR b KQq - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_white_castling_queen() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/R3K3 w Q - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_white_castling_king() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K2R w K - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_black_castling_queen() {
        let board = Board::from_fen("r3k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w q - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_black_castling_king() {
        let board = Board::from_fen("4k1nr/pppppppp/8/8/8/8/PPPPPPPP/4K3 w k - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_black_side_to_move() {
        let board = Board::from_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 b - - 0 1");
        let compressed = Compact::encode(&board);
        let newboard = Compact::decode(&compressed, false);
        assert!(board == newboard);
    }

    #[test]
    fn packed_board_from_fen_encode_decode() {
        let compressed =
            Compact::encode_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1", false);
        let newboard = Compact::decode(&compressed, false);
        assert_eq!(
            "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1",
            newboard.get_fen(true)
        );
        assert_eq!(std::mem::size_of_val(&compressed), 24);
    }

    // -- Zobrist Hash (ported from tests/hash.cpp) --

    #[test]
    fn zobrist_hash_startpos() {
        let mut b = Board::new();
        b.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        assert_eq!(b.hash(), 4842386851105250641);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e2e4");
        assert_eq!(b.zobrist_after(mv, true), 5738310265888351732);
        assert_eq!(b.zobrist_after(mv, false), 5738310265888351732);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 5738310265888351732);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "d7d5");
        assert_eq!(b.zobrist_after(mv, true), 3122418261455772958);
        assert_eq!(b.zobrist_after(mv, false), 3122418261455772958);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 3122418261455772958);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e4e5");
        assert_eq!(b.zobrist_after(mv, true), 13025505418230231218);
        assert_eq!(b.zobrist_after(mv, false), 13025505418230231218);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 13025505418230231218);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "f7f5");
        assert_eq!(b.zobrist_after(mv, true), 10684557267705764058);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 10684557267705764058);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e1e2");
        assert_eq!(b.zobrist_after(mv, true), 96009835303415975);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 96009835303415975);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e8f7");
        assert_eq!(b.zobrist_after(mv, true), 14905950545998454122);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 14905950545998454122);
    }

    #[test]
    fn zobrist_hash_second_position() {
        let mut b = Board::new();
        b.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "a2a4");
        assert_eq!(b.zobrist_after(mv, true), 12816042223619445593);
        b.make_move(mv, false);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "b7b5");
        assert_eq!(b.zobrist_after(mv, true), 2346256875222515170);
        b.make_move(mv, false);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "h2h4");
        assert_eq!(b.zobrist_after(mv, true), 4494335892303760455);
        b.make_move(mv, false);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "b5b4");
        assert_eq!(b.zobrist_after(mv, true), 7775034959770028709);
        b.make_move(mv, false);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "c2c4");
        assert_eq!(b.zobrist_after(mv, true), 13647278551367898716);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 13647278551367898716);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "b4c3");
        assert_eq!(b.zobrist_after(mv, true), 14207649631268237067);
        b.make_move(mv, false);

        let mv = crate::variants::chessport::uci::uci_to_move(&b, "a1a3");
        assert_eq!(b.zobrist_after(mv, true), 9307180103898376204);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 9307180103898376204);
    }

    #[test]
    fn zobrist_hash_white_castling() {
        let mut b = Board::new();

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e1g1");
        assert_eq!(b.zobrist_after(mv, true), 16239684590101438582);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 16239684590101438582);
        b.set_fen("r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
        assert_eq!(b.hash(), 16239684590101438582);

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e1c1");
        assert_eq!(b.zobrist_after(mv, true), 13508779039234121603);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 13508779039234121603);
        b.set_fen("r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1");
        assert_eq!(b.hash(), 13508779039234121603);

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e1f2");
        assert_eq!(b.zobrist_after(mv, true), 16083204071895712714);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 16083204071895712714);

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e1d2");
        assert_eq!(b.zobrist_after(mv, true), 15967657394076784752);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 15967657394076784752);

        b.set_fen("r3k3/8/8/8/8/8/8/4K2R w Kq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "h1h2");
        assert_eq!(b.zobrist_after(mv, true), 9325877987920442636);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 9325877987920442636);
    }

    #[test]
    fn zobrist_hash_black_castling() {
        let mut b = Board::new();

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e8g8");
        assert_eq!(b.zobrist_after(mv, true), 7140796001507982578);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 7140796001507982578);

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e8c8");
        assert_eq!(b.zobrist_after(mv, true), 3770495963768034670);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 3770495963768034670);

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e8f7");
        assert_eq!(b.zobrist_after(mv, true), 8348625604867404572);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 8348625604867404572);

        b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e8d7");
        assert_eq!(b.zobrist_after(mv, true), 14166481666468855000);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 14166481666468855000);

        b.set_fen("r3k3/8/8/3B4/8/8/8/4K3 w q - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "d5a8");
        assert_eq!(b.zobrist_after(mv, true), 18057338517025114558);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 18057338517025114558);

        b.set_fen("r3k3/8/8/8/8/8/8/4K2R b Kq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "a8a7");
        assert_eq!(b.zobrist_after(mv, true), 17497953925507948850);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 17497953925507948850);
    }

    #[test]
    fn zobrist_hash_enpassant() {
        let mut b = Board::new();

        b.set_fen("rnbqkbnr/pppppppp/8/5P2/8/8/PPPPP1PP/RNBQKBNR b KQkq - 0 1");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e7e5");
        assert_eq!(b.zobrist_after(mv, true), 1407624778778493576);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 1407624778778493576);

        b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "f5e6");
        assert_eq!(b.zobrist_after(mv, true), 18079190832899834313);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 18079190832899834313);

        b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "e2e3");
        assert_eq!(b.zobrist_after(mv, true), 15799066465067590645);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 15799066465067590645);

        b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
        let mv = crate::variants::chessport::uci::uci_to_move(&b, "f5f6");
        assert_eq!(b.zobrist_after(mv, true), 15030489466259003258);
        b.make_move(mv, false);
        assert_eq!(b.hash(), 15030489466259003258);
    }

    #[test]
    fn zobrist_hash_null_move() {
        let mut b = Board::new();
        b.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        assert_eq!(
            b.zobrist_after(Move::from_raw(Move::NULL_MOVE), true),
            18315901701230846274
        );
        b.make_null_move();
        assert_eq!(b.hash(), 18315901701230846274);
    }

    struct Perft {
        board: Board,
    }

    impl Perft {
        fn new(board: Board) -> Self {
            Self { board }
        }

        fn perft(&mut self, depth: i32) -> u64 {
            let mut moves = Movelist::new();
            movegen::legalmoves(
                &mut moves,
                &self.board,
                movegen::MoveGenType::All,
                movegen::PieceGenType::ALL,
            );

            if depth == 1 {
                return moves.size() as u64;
            }

            let mut nodes = 0;

            for &m in moves.iter() {
                // Consistency check: gives_check vs in_check after move
                let gives_check = self.board.gives_check(m) != CheckType::NoCheck;

                self.board.make_move(m, false);

                if gives_check != self.board.in_check() {
                    panic!(
                        "gives_check() and in_check() are inconsistent at FEN: {}",
                        self.board.get_fen(true)
                    );
                }

                nodes += self.perft(depth - 1);
                self.board.unmake_move(m);
            }

            nodes
        }

        fn bench_perft(&mut self, depth: i32, expected_node_count: u64) {
            let start = Instant::now();
            let nodes = self.perft(depth);
            let duration = start.elapsed();
            let ms = duration.as_millis() as u64;

            println!(
                "depth {:<2} time {:<5} nodes {:<12} nps {:<9} fen {}",
                depth,
                ms,
                nodes,
                (nodes * 1000) / (ms + 1),
                self.board.get_fen(true)
            );

            assert_eq!(nodes, expected_node_count);
        }
    }

    fn run_test(fen: &str, depth: i32, expected: u64, is_960: bool) {
        let mut board = Board::from_fen(fen);
        if is_960 {
            board.set_960(true);
        }
        let mut perft = Perft::new(board);
        perft.bench_perft(depth, expected);
    }

    #[test]
    fn start_pos() {
        run_test(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            7,
            3195901860,
            false,
        );
    }

    #[test]
    fn kiwipete() {
        run_test(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
            5,
            193690690,
            false,
        );
    }

    #[test]
    fn position_3() {
        run_test(
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
            7,
            178633661,
            false,
        );
    }

    #[test]
    fn position_4() {
        run_test(
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            6,
            706045033,
            false,
        );
    }

    #[test]
    fn position_5() {
        run_test(
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            5,
            89941194,
            false,
        );
    }

    #[test]
    fn position_6() {
        run_test(
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
            5,
            164075551,
            false,
        );
    }
    fn run_test_960(fen: &str, depth: i32, expected: u64) {
        let mut board = Board::from_fen(fen);
        board.set_960(true);
        let mut perft = Perft::new(board);
        perft.bench_perft(depth, expected);
    }

    #[test]
    fn frc_start_pos() {
        run_test_960(
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w AHah - 0 1",
            6,
            119060324,
        );
    }

    #[test]
    fn frc_pos_2() {
        run_test_960(
            "1rqbkrbn/1ppppp1p/1n6/p1N3p1/8/2P4P/PP1PPPP1/1RQBKRBN w FBfb - 0 9",
            6,
            191762235,
        );
    }

    #[test]
    fn frc_pos_3() {
        run_test_960(
            "rbbqn1kr/pp2p1pp/6n1/2pp1p2/2P4P/P7/BP1PPPP1/R1BQNNKR w HAha - 0 9",
            6,
            924181432,
        );
    }

    #[test]
    fn frc_pos_4() {
        run_test_960(
            "rqbbknr1/1ppp2pp/p5n1/4pp2/P7/1PP5/1Q1PPPPP/R1BBKNRN w GAga - 0 9",
            6,
            308553169,
        );
    }

    #[test]
    fn frc_pos_5() {
        run_test_960(
            "4rrb1/1kp3b1/1p1p4/pP1Pn2p/5p2/1PR2P2/2P1NB1P/2KR1B2 w D - 0 21",
            6,
            872323796,
        );
    }

    #[test]
    fn frc_pos_6() {
        run_test_960(
            "1rkr3b/1ppn3p/3pB1n1/6q1/R2P4/4N1P1/1P5P/2KRQ1B1 b Dbd - 0 14",
            6,
            2678022813,
        );
    }

    #[test]
    fn frc_pos_7() {
        run_test_960(
            "qbbnrkr1/p1pppppp/1p4n1/8/2P5/6N1/PPNPPPPP/1BRKBRQ1 b FCge - 1 3",
            6,
            521301336,
        );
    }

    #[test]
    fn frc_castle_depth_2() {
        run_test_960(
            "rr6/2kpp3/1ppn2p1/p2b1q1p/P4P1P/1PNN2P1/2PP4/1K2R2R b E - 1 20",
            2,
            1438,
        );
    }

    #[test]
    fn frc_castle_depth_3() {
        run_test_960(
            "rr6/2kpp3/1ppn2p1/p2b1q1p/P4P1P/1PNN2P1/2PP4/1K2RR2 w E - 0 20",
            3,
            37340,
        );
    }

    #[test]
    fn frc_castle_depth_4_black() {
        run_test_960(
            "rr6/2kpp3/1ppnb1p1/p2Q1q1p/P4P1P/1PNN2P1/2PP4/1K2RR2 b E - 2 19",
            4,
            2237725,
        );
    }

    #[test]
    fn frc_castle_depth_6_large() {
        run_test_960(
            "rr6/2kpp3/1ppnb1p1/p4q1p/P4P1P/1PNN2P1/2PP2Q1/1K2RR2 w E - 1 19",
            6,
            2998685421,
        );
    }
}
