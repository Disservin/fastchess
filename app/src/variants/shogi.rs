use itertools::iproduct;
use lazy_static::lazy_static;
use std::fmt;
use std::ops;

// taken from https://github.com/87flowers/shogitest/blob/main/src/shogi.rs

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum Color {
    Sente,
    Gote,
}

impl Color {
    pub fn to_index(self) -> usize {
        match self {
            Color::Sente => 0,
            Color::Gote => 1,
        }
    }

    pub fn parse(s: &str) -> Option<Color> {
        match s {
            "b" => Some(Color::Sente),
            "w" => Some(Color::Gote),
            _ => None,
        }
    }
}

impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Color::Sente => "b",
                Color::Gote => "w",
            }
        )
    }
}

impl ops::Not for Color {
    type Output = Self;

    fn not(self) -> Self::Output {
        match self {
            Color::Sente => Color::Gote,
            Color::Gote => Color::Sente,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
#[repr(transparent)]
pub(crate) struct Square(u8);

impl Square {
    pub fn new(file: i8, rank: i8) -> Option<Square> {
        let value = rank * 9 + file;
        if (0..=8).contains(&rank) && (0..=8).contains(&file) {
            Some(Square(value as u8))
        } else {
            None
        }
    }

    pub fn parse(c0: u8, c1: u8) -> Option<Square> {
        if !(b'1'..=b'9').contains(&c0) || !(b'a'..=b'i').contains(&c1) {
            return None;
        }
        let file = c0 - b'1';
        let rank = c1 - b'a';
        Self::new(file as i8, rank as i8)
    }

    pub fn from_fen_ordering(i: usize) -> Option<Square> {
        let file = (8 - i % 9) as i8;
        let rank = (i / 9) as i8;
        Square::new(file, rank)
    }

    pub fn file(self) -> i8 {
        (self.0 % 9) as i8
    }

    pub fn rank(self) -> i8 {
        (self.0 / 9) as i8
    }

    pub fn to_index(self) -> usize {
        self.0 as usize
    }

    pub fn normalize_to_sente(self, current_color: Color) -> Square {
        match current_color {
            Color::Sente => self,
            Color::Gote => Square::new(self.file(), 8 - self.rank()).unwrap(),
        }
    }

    pub fn is_promo_square(self, color: Color) -> bool {
        self.normalize_to_sente(color).0 < 27
    }
}

impl ops::Sub for Square {
    type Output = Delta;

    fn sub(self, other: Self) -> Self::Output {
        Delta {
            file: self.file() - other.file(),
            rank: self.rank() - other.rank(),
        }
    }
}

impl ops::Add<Delta> for Square {
    type Output = Option<Square>;

    fn add(self, other: Delta) -> Self::Output {
        Square::new(self.file() + other.file, self.rank() + other.rank)
    }
}

impl fmt::Display for Square {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}{}",
            (b'1' + (self.file() as u8)) as char,
            (b'a' + (self.rank() as u8)) as char,
        )
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub(crate) struct Delta {
    pub file: i8,
    pub rank: i8,
}

impl Delta {
    pub fn normalize_to_sente(self, piece_color: Color) -> Delta {
        match piece_color {
            Color::Sente => self,
            Color::Gote => Delta {
                file: self.file,
                rank: -self.rank,
            },
        }
    }

    pub fn signum(self) -> Delta {
        Delta {
            file: self.file.signum(),
            rank: self.rank.signum(),
        }
    }

    pub fn is_ring(self) -> bool {
        (self.file != 0 || self.rank != 0) && self.file.abs() <= 1 && self.rank.abs() <= 1
    }

    pub fn could_be_piece_move(self, color: Color, pt: PieceType) -> bool {
        let d = self.normalize_to_sente(color);
        if d.rank == 0 && d.file == 0 {
            return false;
        }
        let is_ring = self.is_ring();
        match pt {
            PieceType::None => false,
            PieceType::King => is_ring,
            PieceType::Rook => d.rank == 0 || d.file == 0,
            PieceType::Dragon => is_ring || d.rank == 0 || d.file == 0,
            PieceType::Bishop => d.rank.abs() == d.file.abs(),
            PieceType::Horse => is_ring || d.rank.abs() == d.file.abs(),
            PieceType::Knight => d.rank == -2 && d.file.abs() == 1,
            PieceType::Lance => d.file == 0 && d.rank < 0,
            PieceType::Pawn => d.file == 0 && d.rank == -1,
            PieceType::Silver => is_ring && (d.rank == -1 || d.rank.abs() == d.file.abs()),
            PieceType::Gold
            | PieceType::Tokin
            | PieceType::NariLance
            | PieceType::NariSilver
            | PieceType::NariKnight => is_ring && (d.rank == -1 || d.rank == 0 || d.file == 0),
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
#[repr(u8)]
pub(crate) enum PieceType {
    #[default]
    None = 0o00,
    Pawn = 0o01,
    Bishop = 0o02,
    Rook = 0o03,
    Lance = 0o04,
    Knight = 0o05,
    Silver = 0o06,
    Gold = 0o07,
    King = 0o10,
    Tokin = 0o11,
    Horse = 0o12,
    Dragon = 0o13,
    NariLance = 0o14,
    NariKnight = 0o15,
    NariSilver = 0o16,
}

impl PieceType {
    pub fn promotable(self) -> bool {
        self as u8 >= Self::Pawn as u8 && self as u8 <= Self::Silver as u8
    }

    pub fn promoted(self) -> bool {
        self as u8 >= Self::Tokin as u8
    }

    pub fn promote(self) -> PieceType {
        if self == Self::None || self == Self::Gold {
            self
        } else {
            // SAFETY: Unpromotables have been excluded above; 0o17 can never be generated here
            unsafe { std::mem::transmute::<u8, PieceType>((self as u8) | 0o10) }
        }
    }

    pub fn demote(self) -> PieceType {
        if !self.promoted() {
            self
        } else {
            // SAFETY: Unpromoteds have been excluded above; all values between 0o00 and 0o07 are valid
            unsafe { std::mem::transmute::<u8, PieceType>((self as u8) & 0o07) }
        }
    }

    pub fn to_str(self, color: Color) -> &'static str {
        match (color, self) {
            (Color::Sente, PieceType::None) => "",
            (Color::Sente, PieceType::Pawn) => "P",
            (Color::Sente, PieceType::Bishop) => "B",
            (Color::Sente, PieceType::Rook) => "R",
            (Color::Sente, PieceType::Lance) => "L",
            (Color::Sente, PieceType::Knight) => "N",
            (Color::Sente, PieceType::Silver) => "S",
            (Color::Sente, PieceType::Gold) => "G",
            (Color::Sente, PieceType::King) => "K",
            (Color::Sente, PieceType::Tokin) => "+P",
            (Color::Sente, PieceType::Horse) => "+B",
            (Color::Sente, PieceType::Dragon) => "+R",
            (Color::Sente, PieceType::NariLance) => "+L",
            (Color::Sente, PieceType::NariKnight) => "+N",
            (Color::Sente, PieceType::NariSilver) => "+S",
            (Color::Gote, PieceType::None) => "",
            (Color::Gote, PieceType::Pawn) => "p",
            (Color::Gote, PieceType::Bishop) => "b",
            (Color::Gote, PieceType::Rook) => "r",
            (Color::Gote, PieceType::Lance) => "l",
            (Color::Gote, PieceType::Knight) => "n",
            (Color::Gote, PieceType::Silver) => "s",
            (Color::Gote, PieceType::Gold) => "g",
            (Color::Gote, PieceType::King) => "k",
            (Color::Gote, PieceType::Tokin) => "+p",
            (Color::Gote, PieceType::Horse) => "+b",
            (Color::Gote, PieceType::Dragon) => "+r",
            (Color::Gote, PieceType::NariLance) => "+l",
            (Color::Gote, PieceType::NariKnight) => "+n",
            (Color::Gote, PieceType::NariSilver) => "+s",
        }
    }
}

impl fmt::Display for PieceType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.to_str(Color::Sente))
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub(crate) enum Move {
    #[default]
    None,
    Win,
    Resign,
    Drop(PieceType, Square),
    Normal {
        from: Square,
        to: Square,
        promo: bool,
    },
}

impl Move {
    pub fn parse(s: &str) -> Option<Move> {
        if s.eq_ignore_ascii_case("null") {
            return Some(Move::None);
        }
        if s.eq_ignore_ascii_case("win") {
            return Some(Move::Win);
        }
        if s.eq_ignore_ascii_case("resign") {
            return Some(Move::Resign);
        }
        let bytes = s.as_bytes();
        if bytes.len() < 4 || bytes.len() > 5 {
            return None;
        }
        if bytes[1] == b'*' {
            if bytes.len() != 4 {
                return None;
            }
            let pt = match bytes[0] {
                b'P' => PieceType::Pawn,
                b'N' => PieceType::Knight,
                b'L' => PieceType::Lance,
                b'S' => PieceType::Silver,
                b'G' => PieceType::Gold,
                b'B' => PieceType::Bishop,
                b'R' => PieceType::Rook,
                _ => return None,
            };
            let to = Square::parse(bytes[2], bytes[3])?;
            Some(Move::Drop(pt, to))
        } else {
            if s.len() == 5 && bytes[4] != b'+' {
                return None;
            }
            let promo = s.len() == 5;
            let from = Square::parse(bytes[0], bytes[1])?;
            let to = Square::parse(bytes[2], bytes[3])?;
            Some(Move::Normal { from, to, promo })
        }
    }
}

impl fmt::Display for Move {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Move::None => write!(f, "null"),
            Move::Win => write!(f, "win"),
            Move::Resign => write!(f, "resign"),
            Move::Normal { from, to, promo } => {
                write!(f, "{from}{to}{}", if *promo { "+" } else { "" })
            }
            Move::Drop(pt, sq) => write!(f, "{pt}*{sq}"),
        }
    }
}

lazy_static! {
    static ref ALL_MOVES: Vec<Move> = {
        let sq: Vec<Square> = (0..81).map(Square).collect();
        let drop_ptypes: Vec<PieceType> = vec![
            PieceType::Pawn,
            PieceType::Bishop,
            PieceType::Rook,
            PieceType::Lance,
            PieceType::Knight,
            PieceType::Silver,
            PieceType::Gold,
        ];
        let bools: Vec<bool> = vec![false, true];
        let mut res = Vec::<Move>::new();
        res.extend(
            iproduct!(&sq, &sq, &bools).map(|(&from, &to, &promo)| Move::Normal {
                from,
                to,
                promo,
            }),
        );
        res.extend(iproduct!(&drop_ptypes, &sq).map(|(&pt, &sq)| Move::Drop(pt, sq)));
        res
    };
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) struct Place(Color, PieceType);

impl Place {
    fn is_empty(self) -> bool {
        self.1 == PieceType::None
    }
}

impl Default for Place {
    fn default() -> Self {
        Place(Color::Sente, PieceType::None)
    }
}

impl fmt::Display for Place {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.1.to_str(self.0),)
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub(crate) struct Hand {
    rook: u8,
    bishop: u8,
    gold: u8,
    silver: u8,
    knight: u8,
    lance: u8,
    pawn: u8,
}

impl Hand {
    pub fn get(&self, pt: PieceType) -> u8 {
        match pt {
            PieceType::Rook => self.rook,
            PieceType::Bishop => self.bishop,
            PieceType::Gold => self.gold,
            PieceType::Silver => self.silver,
            PieceType::Knight => self.knight,
            PieceType::Lance => self.lance,
            PieceType::Pawn => self.pawn,
            _ => panic!(),
        }
    }

    pub fn get_mut(&mut self, pt: PieceType) -> &mut u8 {
        match pt {
            PieceType::Rook => &mut self.rook,
            PieceType::Bishop => &mut self.bishop,
            PieceType::Gold => &mut self.gold,
            PieceType::Silver => &mut self.silver,
            PieceType::Knight => &mut self.knight,
            PieceType::Lance => &mut self.lance,
            PieceType::Pawn => &mut self.pawn,
            _ => panic!(),
        }
    }

    fn set_from_parse(&mut self, pt: PieceType, modifier: Option<usize>) {
        let count = modifier.unwrap_or(1) as u8;
        match pt {
            PieceType::Rook => self.rook = count,
            PieceType::Bishop => self.bishop = count,
            PieceType::Gold => self.gold = count,
            PieceType::Silver => self.silver = count,
            PieceType::Knight => self.knight = count,
            PieceType::Lance => self.lance = count,
            PieceType::Pawn => self.pawn = count,
            _ => panic!(),
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum CheckState {
    None,
    Check,
    Checkmate,
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) struct Position {
    board: [Place; 81],
    hand: [Hand; 2],
    stm: Color,
    ply: usize,
}

impl Default for Position {
    fn default() -> Self {
        Position::parse("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1").unwrap()
    }
}

impl Position {
    pub fn is_clone_of(&self, other: &Position) -> bool {
        self.board == other.board && self.hand == other.hand && self.stm == other.stm
    }

    pub fn king_sq(&self, king_color: Color) -> Square {
        Square(
            self.board
                .iter()
                .position(|&p| p == Place(king_color, PieceType::King))
                .unwrap() as u8,
        )
    }

    pub fn is_empty(&self, sq: Square) -> bool {
        self.board[sq.to_index()].1 == PieceType::None
    }

    pub fn is_enemy(&self, sq: Square) -> bool {
        let place = self.board[sq.to_index()];
        place.0 != self.stm && place.1 != PieceType::None
    }

    pub fn is_friendly(&self, sq: Square) -> bool {
        let place = self.board[sq.to_index()];
        place.0 == self.stm && place.1 != PieceType::None
    }

    pub fn is_in_check(&self) -> bool {
        let king_sq = self.king_sq(self.stm);
        (0..81)
            .map(Square)
            .filter(|&sq| self.is_enemy(sq))
            .any(|sq| self.piece_has_ray_to(sq, king_sq))
    }

    pub fn get_check_state(&self) -> CheckState {
        if !self.is_in_check() {
            CheckState::None
        } else if self.has_legal_move() {
            CheckState::Check
        } else {
            CheckState::Checkmate
        }
    }

    // 二歩
    fn is_nifu(&self, file: i8) -> bool {
        let example_pawn = Place(self.stm, PieceType::Pawn);
        (0..=8)
            .map(|rank| Square::new(file, rank).unwrap())
            .any(|sq| self.board[sq.to_index()] == example_pawn)
    }

    // 行き所のない駒
    fn is_ikidokorononai(color: Color, pt: PieceType, sq: Square) -> bool {
        let normalized_sq = sq.normalize_to_sente(color);
        match pt {
            PieceType::None => true,
            PieceType::Pawn | PieceType::Lance => normalized_sq.rank() <= 0,
            PieceType::Knight => normalized_sq.rank() <= 1,
            _ => false,
        }
    }

    pub fn is_legal(&self, m: Move) -> bool {
        self.do_move(m).is_some()
    }

    pub fn do_move(&self, m: Move) -> Option<Position> {
        let mut new_pos = *self;
        match m {
            Move::None => return None,
            Move::Win => return None,
            Move::Resign => return None,
            Move::Drop(ptype, sq) => {
                if !self.is_empty(sq)
                    // Invalid piece type to drop
                    || ptype.promoted() || ptype == PieceType::None || ptype == PieceType::King
                    // Illegal move rule: 二歩
                    || (ptype == PieceType::Pawn && self.is_nifu(sq.file()))
                    // Illegal move rule: 行き所のない駒
                    || Position::is_ikidokorononai(self.stm, ptype, sq)
                {
                    return None;
                }

                let piece_in_hand: &mut u8 = new_pos.hand[self.stm.to_index()].get_mut(ptype);
                if *piece_in_hand == 0 {
                    return None;
                }
                *piece_in_hand -= 1;

                new_pos.board[sq.to_index()] = Place(new_pos.stm, ptype)
            }
            Move::Normal { from, to, promo } => {
                let is_capture = !self.is_empty(to);
                let ptype = self.board[from.to_index()].1;

                // Invalid moves
                if from == to {
                    return None;
                }
                if !self.is_friendly(from) {
                    return None;
                }
                if promo && !from.is_promo_square(self.stm) && !to.is_promo_square(self.stm) {
                    return None;
                }
                if promo && !ptype.promotable() {
                    return None;
                }
                if is_capture && !self.is_enemy(to) {
                    return None;
                }

                // Illegal move rule: 行き所のない駒
                if !promo && Position::is_ikidokorononai(self.stm, ptype, to) {
                    return None;
                }

                // Check piece movement
                if !self.piece_has_ray_to(from, to) {
                    return None;
                }

                // Move on board
                let place = Place(self.stm, if promo { ptype.promote() } else { ptype });
                if is_capture {
                    let hand_ptype = self.board[to.to_index()].1.demote();
                    *new_pos.hand[self.stm.to_index()].get_mut(hand_ptype) += 1;
                }
                new_pos.board[to.to_index()] = place;
                new_pos.board[from.to_index()] = Place::default();
            }
        }

        // Check if we left ourselves in check
        if new_pos.is_in_check() {
            return None;
        }

        new_pos.stm = !new_pos.stm;
        new_pos.ply += 1;

        // Illegal move rule: 打ち歩詰め (uchifuzume - pawn drop checkmate)
        if let Move::Drop(PieceType::Pawn, _) = m {
            if new_pos.get_check_state() == CheckState::Checkmate {
                return None;
            }
        }

        Some(new_pos)
    }

    pub fn piece_has_ray_to(&self, from: Square, to: Square) -> bool {
        let place = self.board[from.to_index()];
        let delta = to - from;
        let delta_valid = delta.could_be_piece_move(place.0, place.1);
        match place.1 {
            PieceType::None => false,
            PieceType::Knight => delta_valid,
            _ => delta_valid && self.is_clear_between(from, to),
        }
    }

    fn is_clear_between(&self, a: Square, b: Square) -> bool {
        let delta = b - a;
        assert!(a != b);
        assert!(delta.file == 0 || delta.rank == 0 || delta.file.abs() == delta.rank.abs());
        let step = delta.signum();
        let mut current = (a + step).unwrap();
        while current != b {
            if !self.is_empty(current) {
                return false;
            }
            current = (current + step).unwrap();
        }
        true
    }

    pub fn has_legal_move(&self) -> bool {
        ALL_MOVES.iter().any(|&m| self.is_legal(m))
    }

    #[cfg(test)]
    pub fn legal_moves(&self) -> Vec<Move> {
        ALL_MOVES
            .iter()
            .filter(|&&m| self.is_legal(m))
            .copied()
            .collect()
    }

    pub fn parse(s: &str) -> Option<Position> {
        let mut it = s.split(' ');
        let board = it.next()?;
        let color = it.next()?;
        let hand = it.next()?;
        let ply = it.next()?;
        if it.next().is_none() {
            Position::parse_parts(board, color, hand, ply)
        } else {
            None
        }
    }

    pub fn parse_parts(board: &str, color: &str, hand: &str, ply: &str) -> Option<Position> {
        Some(Position {
            board: Position::parse_board(board)?,
            hand: Position::parse_hand(hand)?,
            stm: Color::parse(color)?,
            ply: ply.parse().ok()?,
        })
    }

    fn parse_board(s: &str) -> Option<[Place; 81]> {
        let mut board = [Place(Color::Sente, PieceType::None); 81];

        let board_str = s.as_bytes();
        let mut place_index: usize = 0;
        let mut i: usize = 0;

        while place_index < 81 && i < board_str.len() {
            let sq = Square::from_fen_ordering(place_index).unwrap();
            let ch = board_str[i];
            match ch {
                b'/' => {
                    if sq.file() != 8 || place_index == 0 {
                        return None;
                    }
                    i += 1;
                    continue;
                }
                b'1'..=b'9' => {
                    place_index += (ch - b'0') as usize;
                    i += 1;
                    continue;
                }
                b'p' => board[sq.to_index()] = Place(Color::Gote, PieceType::Pawn),
                b'b' => board[sq.to_index()] = Place(Color::Gote, PieceType::Bishop),
                b'r' => board[sq.to_index()] = Place(Color::Gote, PieceType::Rook),
                b'l' => board[sq.to_index()] = Place(Color::Gote, PieceType::Lance),
                b'n' => board[sq.to_index()] = Place(Color::Gote, PieceType::Knight),
                b's' => board[sq.to_index()] = Place(Color::Gote, PieceType::Silver),
                b'g' => board[sq.to_index()] = Place(Color::Gote, PieceType::Gold),
                b'k' => board[sq.to_index()] = Place(Color::Gote, PieceType::King),
                b'P' => board[sq.to_index()] = Place(Color::Sente, PieceType::Pawn),
                b'B' => board[sq.to_index()] = Place(Color::Sente, PieceType::Bishop),
                b'R' => board[sq.to_index()] = Place(Color::Sente, PieceType::Rook),
                b'L' => board[sq.to_index()] = Place(Color::Sente, PieceType::Lance),
                b'N' => board[sq.to_index()] = Place(Color::Sente, PieceType::Knight),
                b'S' => board[sq.to_index()] = Place(Color::Sente, PieceType::Silver),
                b'G' => board[sq.to_index()] = Place(Color::Sente, PieceType::Gold),
                b'K' => board[sq.to_index()] = Place(Color::Sente, PieceType::King),
                b'+' => {
                    i += 1;
                    if i >= board_str.len() {
                        return None;
                    }
                    match board_str[i] {
                        b'p' => board[sq.to_index()] = Place(Color::Gote, PieceType::Tokin),
                        b'b' => board[sq.to_index()] = Place(Color::Gote, PieceType::Horse),
                        b'r' => board[sq.to_index()] = Place(Color::Gote, PieceType::Dragon),
                        b'l' => board[sq.to_index()] = Place(Color::Gote, PieceType::NariLance),
                        b'n' => board[sq.to_index()] = Place(Color::Gote, PieceType::NariKnight),
                        b's' => board[sq.to_index()] = Place(Color::Gote, PieceType::NariSilver),
                        b'P' => board[sq.to_index()] = Place(Color::Sente, PieceType::Tokin),
                        b'B' => board[sq.to_index()] = Place(Color::Sente, PieceType::Horse),
                        b'R' => board[sq.to_index()] = Place(Color::Sente, PieceType::Dragon),
                        b'L' => board[sq.to_index()] = Place(Color::Sente, PieceType::NariLance),
                        b'N' => board[sq.to_index()] = Place(Color::Sente, PieceType::NariKnight),
                        b'S' => board[sq.to_index()] = Place(Color::Sente, PieceType::NariSilver),
                        _ => return None,
                    }
                }
                _ => return None,
            }
            i += 1;
            place_index += 1;
        }

        if place_index != 81 || i != board_str.len() {
            return None;
        }

        Some(board)
    }

    fn parse_hand(s: &str) -> Option<[Hand; 2]> {
        let mut hand = [Hand::default(); 2];

        if s == "-" {
            return Some(hand);
        }

        let mut modifier: Option<usize> = None;

        for ch in s.bytes() {
            match ch {
                b'0'..=b'9' => {
                    if modifier.is_none() && ch == b'0' {
                        return None;
                    }
                    modifier = Some(modifier.unwrap_or(0) * 10 + (ch - b'0') as usize);
                    if modifier > Some(18) {
                        return None;
                    }
                    continue;
                }
                b'p' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Pawn, modifier),
                b'b' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Bishop, modifier),
                b'r' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Rook, modifier),
                b'l' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Lance, modifier),
                b'n' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Knight, modifier),
                b's' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Silver, modifier),
                b'g' => hand[Color::Gote.to_index()].set_from_parse(PieceType::Gold, modifier),
                b'P' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Pawn, modifier),
                b'B' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Bishop, modifier),
                b'R' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Rook, modifier),
                b'L' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Lance, modifier),
                b'N' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Knight, modifier),
                b'S' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Silver, modifier),
                b'G' => hand[Color::Sente.to_index()].set_from_parse(PieceType::Gold, modifier),
                _ => return None,
            }
            modifier = None;
        }

        if modifier.is_some() {
            return None;
        }

        Some(hand)
    }

    #[cfg(test)]
    fn perft(&self, depth: usize, print: bool) -> u64 {
        if depth == 0 {
            return 1;
        }

        let moves = self.legal_moves();
        if depth == 1 && !print {
            return moves.len() as u64;
        }

        moves
            .iter()
            .map(|&m| {
                let child_position = self.do_move(m).unwrap();
                let child = child_position.perft(depth - 1, false);
                if print {
                    println!("{m}\t{child}");
                }
                child
            })
            .sum()
    }
}

impl fmt::Display for Position {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Board
        {
            let mut blanks = 0;
            for i in 0..81 {
                let sq = Square::from_fen_ordering(i).unwrap();
                let place = self.board[sq.to_index()];
                if place.is_empty() {
                    blanks += 1;
                } else {
                    if blanks != 0 {
                        write!(f, "{blanks}")?;
                        blanks = 0;
                    }
                    write!(f, "{place}")?;
                }
                if sq.file() == 0 {
                    if blanks != 0 {
                        write!(f, "{blanks}")?;
                        blanks = 0;
                    }
                    if i != 80 {
                        write!(f, "/")?;
                    }
                }
            }
        }

        // Side to move
        write!(f, " {} ", self.stm)?;

        // Hand
        {
            let mut wrote_hand = false;
            let mut op = |count, ch| {
                if count != 0 {
                    wrote_hand = true;
                    if count == 1 {
                        write!(f, "{ch}")
                    } else {
                        write!(f, "{count}{ch}")
                    }
                } else {
                    Ok(())
                }
            };

            op(self.hand[0].rook, 'R')?;
            op(self.hand[0].bishop, 'B')?;
            op(self.hand[0].gold, 'G')?;
            op(self.hand[0].silver, 'S')?;
            op(self.hand[0].knight, 'N')?;
            op(self.hand[0].lance, 'L')?;
            op(self.hand[0].pawn, 'P')?;
            op(self.hand[1].rook, 'r')?;
            op(self.hand[1].bishop, 'b')?;
            op(self.hand[1].gold, 'g')?;
            op(self.hand[1].silver, 's')?;
            op(self.hand[1].knight, 'n')?;
            op(self.hand[1].lance, 'l')?;
            op(self.hand[1].pawn, 'p')?;

            if !wrote_hand {
                write!(f, "-")?;
            }
        }

        // Ply
        write!(f, " {}", self.ply)?;

        Ok(())
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum GameOutcome {
    Undetermined,
    Checkmated(Color),
    WinInImpasse(Color),
    DrawBySennichite,
    LossByPerpetual(Color),
    LossByIllegal(Color),
    Resignation(Color),
}

#[derive(Clone, Debug)]
pub(crate) struct Game {
    current_position: Position,
    moves: Vec<Move>,
    history: Vec<Position>,
    last_not_in_check_ply: [isize; 2],
}

impl Game {
    pub fn new(startpos: Position) -> Game {
        assert!(!startpos.is_in_check());
        Game {
            current_position: startpos,
            moves: vec![],
            history: vec![startpos],
            last_not_in_check_ply: [-1, -1],
        }
    }

    pub fn stm(&self) -> Color {
        self.current_position.stm
    }

    pub fn usi_string(&self) -> String {
        let mut string = format!("sfen {}", self.history[0]);
        if !self.moves.is_empty() {
            string += " moves";
            string.extend(self.moves.iter().map(|m| format!(" {m}")));
        }
        string
    }

    pub fn do_move(&mut self, m: Move) -> GameOutcome {
        let stm = self.current_position.stm;

        self.moves.push(m);

        if m == Move::Resign {
            return GameOutcome::Resignation(stm);
        }

        if m == Move::Win {
            return if self.valid_impasse_win_declaration() {
                GameOutcome::WinInImpasse(stm)
            } else {
                GameOutcome::LossByIllegal(stm)
            };
        }

        if let Some(next_position) = self.current_position.do_move(m) {
            self.current_position = next_position;
            self.history.push(next_position);
        } else {
            return GameOutcome::LossByIllegal(stm);
        }

        assert!(stm != self.current_position.stm);
        let stm = self.current_position.stm;

        match self.current_position.get_check_state() {
            CheckState::None => {
                self.last_not_in_check_ply[stm.to_index()] = (self.history.len() - 1) as isize
            }
            CheckState::Check => {}
            CheckState::Checkmate => return GameOutcome::Checkmated(stm),
        }

        let (num_clones, first_clone) = {
            let mut num_clones = 0;
            let mut i = self.history.len() - 1;
            let mut first_clone = i;
            loop {
                if self.history[i].is_clone_of(&self.current_position) {
                    num_clones += 1;
                    first_clone = i;
                }
                if i < 2 {
                    break;
                }
                i -= 2;
            }
            (num_clones, first_clone as isize)
        };

        if num_clones < 4 {
            GameOutcome::Undetermined
        } else if self.last_not_in_check_ply[stm.to_index()] < first_clone {
            GameOutcome::LossByPerpetual(!stm)
        } else if self.last_not_in_check_ply[(!stm).to_index()] < first_clone {
            GameOutcome::LossByPerpetual(stm)
        } else {
            GameOutcome::DrawBySennichite
        }
    }

    fn valid_impasse_win_declaration(&self) -> bool {
        let stm = self.current_position.stm;
        let pos = self.current_position;

        let ptype_points = |pt: &PieceType| match pt.demote() {
            PieceType::None => 0,
            PieceType::King => 0,
            PieceType::Rook | PieceType::Bishop => 5,
            PieceType::Pawn
            | PieceType::Lance
            | PieceType::Knight
            | PieceType::Silver
            | PieceType::Gold => 1,
            _ => panic!("should be unreachable"),
        };

        if !pos.king_sq(stm).is_promo_square(stm) {
            return false;
        }
        if pos.get_check_state() != CheckState::None {
            return false;
        }

        let pieces: Vec<PieceType> = pos
            .board
            .iter()
            .filter(|p| p.0 == stm && p.1 != PieceType::None)
            .map(|p| p.1)
            .collect();

        let piece_count = pieces.len();
        let board_points: usize = pieces.iter().map(ptype_points).sum();
        let hand_points: usize = [
            PieceType::Rook,
            PieceType::Bishop,
            PieceType::Pawn,
            PieceType::Lance,
            PieceType::Knight,
            PieceType::Silver,
            PieceType::Gold,
        ]
        .iter()
        .map(|pt| pos.hand[stm.to_index()].get(*pt) as usize * ptype_points(pt))
        .sum();
        let points = board_points + hand_points;

        match stm {
            Color::Sente => piece_count >= 10 && points >= 28,
            Color::Gote => piece_count >= 10 && points >= 27,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn move_parse_test() {
        assert_eq!(
            Move::parse("1a2b+").unwrap(),
            Move::Normal {
                from: Square::parse(b'1', b'a').unwrap(),
                to: Square::parse(b'2', b'b').unwrap(),
                promo: true
            }
        )
    }

    #[test]
    fn roundtrip_fens() {
        let cases = vec![
            "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
            "lnsgk2nl/1r4gs1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1SG4R1/LN2KGSNL b Bb 1",
            "ln1g5/1r2S1k2/p2pppn2/2ps2p2/1p7/2P6/PPSPPPPLP/2G2K1pr/LN4G1b w BGSLPnp 62",
            "8l/1l+R2P3/p2pBG1pp/kps1p4/Nn1P2G2/P1P1P2PP/1PS6/1KSG3+r1/LN2+p3L w Sbgn3p 124",
            "ln1g3nl/2s2k1+P1/p3pg3/1np2p2p/3p5/1SP3P1P/P1KPPSp2/2G6/L2b1G2L w RBSN2Pr2p 66",
        ];
        for case in cases {
            let position = Position::parse(case).unwrap();
            let sfen = format!("{position}");
            assert_eq!(case, sfen);
        }
    }

    #[test]
    fn uchifuzume() {
        let cases = vec![
            ("9/9/7gp/7pk/9/7G1/9/PPPPPPPP1/K8 b P 1", true, "P*1e"),
            ("9/9/7pp/7sk/9/7G1/9/PPPPPPPP1/K8 b P 1", false, "P*1e"),
            ("9/9/8p/6K1k/9/7G1/9/PPPPPPPP1/9 b P 1", true, "P*1e"),
            ("9/9/8p/6K1k/9/9/9/PPPPPPPP1/9 b P 1", false, "P*1e"),
            ("9/9/7gp/1R5gk/9/7G1/9/PPPPPPPP1/K8 b P 1", true, "P*1e"),
            ("9/9/7gp/7gk/9/7G1/9/PPPPPPPP1/K8 b P 1", false, "P*1e"),
        ];
        for (sfen, is_uchifuzume, mstr) in cases {
            let position = Position::parse(sfen).unwrap();
            let m = Move::parse(mstr).unwrap();
            assert_eq!(is_uchifuzume, !position.is_legal(m));
        }
    }

    #[test]
    fn remember_to_promote() {
        let sfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
        let position = Position::parse(sfen).unwrap();
        let position = position.do_move(Move::parse("7g7f").unwrap()).unwrap();
        let position = position.do_move(Move::parse("5a4b").unwrap()).unwrap();
        let position = position.do_move(Move::parse("8h3c+").unwrap()).unwrap();
        assert_eq!(false, position.is_legal(Move::parse("4b3b").unwrap()));
    }

    #[test]
    fn captures_add_pieces_to_hand() {
        let sfen = "8l/1l+R2P3/p2pBG1pp/kps1p4/Nn1P2G2/P1P1P2PP/1PS6/1KSG3+r1/LN2+p3L w Sbgn3p 124";
        let position = Position::parse(sfen).unwrap();
        let position = position.do_move(Move::parse("2h1i").unwrap()).unwrap();
        assert_eq!(22380, position.perft(2, false));
    }

    fn test_perft(sfen: &str, numbers: Vec<u64>) {
        let position = Position::parse(sfen).unwrap();
        for (depth, number) in numbers.iter().enumerate() {
            println!("Testing perft on {sfen} depth={depth} number={number}");
            assert_eq!(*number, position.perft(depth, false));
        }
    }

    #[test]
    fn test_perft_startpos() {
        test_perft(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
            vec![1, 30, 900, 25470, 719731],
        );
    }

    #[test]
    fn test_perft_matsuri() {
        test_perft(
            "l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w GR5pnsg 1",
            vec![1, 207, 28684, 4809015],
        );
    }

    #[test]
    fn test_perft_pos1() {
        test_perft(
            "8l/1l+R2P3/p2pBG1pp/kps1p4/Nn1P2G2/P1P1P2PP/1PS6/1KSG3+r1/LN2+p3L w Sbgn3p 124",
            vec![1, 178, 18041, 2552846],
        );
    }

    #[test]
    fn test_perft_alldrops() {
        test_perft(
            "9/9/9/3k5/9/5K3/9/9/9 b RB2G2S2N2L9Prb2g2s2n2l9p 1",
            vec![1, 524, 248257], // 112911856
        );
    }

    #[test]
    fn test_outcome() {
        let cases = vec![
            (
                "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
                "2h7h 8b9b 7h6h 9b8b 6h7h 8b9b 7h6h 9b8b 6h7h 8b9b 7h6h 9b8b",
                "6h7h",
                GameOutcome::DrawBySennichite,
            ),
            (
                "l7l/g1S6/k1n4pp/ppN3p2/2p4P1/P2P1bP2/KSN5P/1G7/L1bS4L w 2RSN5P2g2p 1",
                "G*8i R*7h 8i8h 7h8h G*8i G*7h 8i8h 7h8h G*8i G*7h 8i8h 7h8h G*8i G*7h 8i8h 7h8h G*8i G*7h",
                "8i8h",
                GameOutcome::DrawBySennichite,
            ),
            (
                "lnsG3Rl/1rg1ks3/p1ppppp1p/9/9/1p7/PPPPPPP1P/1B7/LNSGKGSNL b BNPp 1",
                "6a5a 5b6b 5a6a 6b5b 6a5a 5b6b 5a6a 6b5b 6a5a 5b6b 5a6a",
                "6b5b",
                GameOutcome::LossByPerpetual(Color::Sente),
            ),
        ];
        for (sfen, moves, last_move, expected_outcome) in cases {
            println!("{sfen}");
            let position = Position::parse(sfen).unwrap();
            let mut game = Game::new(position);
            for mstr in moves.split(' ') {
                let m = Move::parse(mstr).unwrap();
                let intermediate_outcome = game.do_move(m);
                println!("{m}: {:?}", intermediate_outcome);
                assert_eq!(intermediate_outcome, GameOutcome::Undetermined);
            }
            let m = Move::parse(last_move).unwrap();
            let final_outcome = game.do_move(m);
            println!("{m}: {:?}", final_outcome);
            assert_eq!(final_outcome, expected_outcome);
        }
    }
}

// ── Game Trait Implementation ────────────────────────────────────────────────

use crate::game::{Color as GameColor, GameStatus};
use crate::types::VariantType;
use crate::variants::GameMove;

/// A shogi move wrapper that implements GameMove.
#[derive(Clone)]
struct ShogiMove {
    inner: Move,
}

impl ShogiMove {
    /// Creates a new shogi move.
    pub fn new(mv: Move) -> Self {
        Self { inner: mv }
    }

    /// Returns the inner move.
    pub fn inner(&self) -> Move {
        self.inner
    }
}

impl GameMove for ShogiMove {
    fn to_uci(&self) -> String {
        self.inner.to_string()
    }

    fn to_san(&self, _game: &dyn crate::variants::Game) -> Option<String> {
        // Shogi uses USI notation as its standard notation
        Some(self.inner.to_string())
    }

    fn to_lan(&self, _game: &dyn crate::variants::Game) -> Option<String> {
        // Shogi uses USI notation as its standard notation
        Some(self.inner.to_string())
    }

    fn clone_box(&self) -> Box<dyn GameMove> {
        Box::new(self.clone())
    }

    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}

/// A shogi game wrapper that implements the Game trait.
#[derive(Clone)]
pub struct ShogiGame {
    game: Game,
    ply_count: u32,
}

impl ShogiGame {
    /// Creates a new shogi game from the standard starting position.
    pub fn new() -> Self {
        Self {
            game: Game::new(Position::default()),
            ply_count: 0,
        }
    }

    /// Creates a new shogi game from a SFEN string.
    pub fn from_sfen(sfen: &str) -> Option<Self> {
        let position = Position::parse(sfen)?;
        Some(Self {
            game: Game::new(position),
            ply_count: 0,
        })
    }

    /// Parses a USI move string.
    fn parse_usi_move(&self, usi: &str) -> Option<ShogiMove> {
        Move::parse(usi).map(ShogiMove::new)
    }

    /// Makes a move and returns true if successful.
    fn make_shogi_move(&mut self, mv: &ShogiMove) -> bool {
        let outcome = self.game.do_move(mv.inner());
        if outcome != GameOutcome::LossByIllegal(self.game.stm()) {
            self.ply_count += 1;
            true
        } else {
            false
        }
    }

    /// Makes a move from a USI string.
    pub fn make_usi_move(&mut self, usi: &str) -> bool {
        if let Some(mv) = self.parse_usi_move(usi) {
            self.make_shogi_move(&mv)
        } else {
            false
        }
    }

    /// Converts a USI move to string representation.
    pub fn usi_to_san(&self, usi: &str) -> Option<String> {
        // Shogi uses USI notation directly
        Some(usi.to_string())
    }

    /// Converts a USI move to string representation.
    pub fn usi_to_lan(&self, usi: &str) -> Option<String> {
        // Shogi uses USI notation directly
        Some(usi.to_string())
    }
}

impl Default for ShogiGame {
    fn default() -> Self {
        Self::new()
    }
}

impl crate::variants::Game for ShogiGame {
    fn clone_box(&self) -> Box<dyn crate::variants::Game> {
        Box::new(self.clone())
    }

    fn variant(&self) -> VariantType {
        VariantType::Shogi
    }

    fn side_to_move(&self) -> GameColor {
        match self.game.stm() {
            Color::Sente => GameColor::First,
            Color::Gote => GameColor::Second,
        }
    }

    fn halfmove_clock(&self) -> u32 {
        0 // Shogi doesn't have a half-move clock
    }

    fn ply_count(&self) -> u32 {
        self.ply_count
    }

    fn status(&self) -> GameStatus {
        // Check for game over conditions
        // For now, we return ongoing - the actual outcome is tracked by do_move
        GameStatus::ONGOING
    }

    fn parse_move(&self, notation: &str) -> Option<Box<dyn GameMove>> {
        self.parse_usi_move(notation)
            .map(|m| Box::new(m) as Box<dyn GameMove>)
    }

    fn make_move(&mut self, mv: &dyn GameMove) -> bool {
        if let Some(shogi_move) = mv.as_any().downcast_ref::<ShogiMove>() {
            self.make_shogi_move(shogi_move)
        } else {
            false
        }
    }

    fn make_move_notation(&mut self, notation: &str) -> bool {
        if let Some(mv) = self.parse_usi_move(notation) {
            self.make_shogi_move(&mv)
        } else {
            false
        }
    }

    fn fen(&self) -> String {
        self.game.usi_string()
    }

    fn move_to_san(&self, notation: &str) -> Option<String> {
        self.usi_to_san(notation)
    }

    fn move_to_lan(&self, notation: &str) -> Option<String> {
        self.usi_to_lan(notation)
    }

    fn convert_move_to_san(&self, mv: &dyn crate::variants::GameMove) -> Option<String> {
        if let Some(shogi_move) = mv.as_any().downcast_ref::<ShogiMove>() {
            // Shogi uses USI notation as its standard notation
            Some(shogi_move.inner().to_string())
        } else {
            None
        }
    }

    fn convert_move_to_lan(&self, mv: &dyn crate::variants::GameMove) -> Option<String> {
        if let Some(shogi_move) = mv.as_any().downcast_ref::<ShogiMove>() {
            // Shogi uses USI notation as its standard notation
            Some(shogi_move.inner().to_string())
        } else {
            None
        }
    }

    fn supports_syzygy(&self) -> bool {
        false
    }

    fn as_chess(&self) -> Option<&crate::variants::chess::ChessGame> {
        None
    }

    fn as_shogi(&self) -> Option<&ShogiGame> {
        Some(self)
    }
}
