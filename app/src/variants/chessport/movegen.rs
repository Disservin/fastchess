use crate::variants::chessport::attacks;
use crate::variants::chessport::bitboard::Bitboard;
use crate::variants::chessport::board::Board;
use crate::variants::chessport::chess_move::Move;
use crate::variants::chessport::color::Color;
use crate::variants::chessport::constants::DEFAULT_CHECKMASK;
use crate::variants::chessport::coords::{Direction, File, Rank, Square};
use crate::variants::chessport::movelist::Movelist;
use crate::variants::chessport::piece::PieceType;

// ---------------------------------------------------------------------------
// PieceGenType flags
// ---------------------------------------------------------------------------

pub struct PieceGenType;

impl PieceGenType {
    pub const PAWN: u32 = 1;
    pub const KNIGHT: u32 = 2;
    pub const BISHOP: u32 = 4;
    pub const ROOK: u32 = 8;
    pub const QUEEN: u32 = 16;
    pub const KING: u32 = 32;
    pub const ALL: u32 = 63;
}

// ---------------------------------------------------------------------------
// MoveGenType
// ---------------------------------------------------------------------------

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
#[repr(u8)]
pub enum MoveGenType {
    All = 0,
    Capture = 1,
    Quiet = 2,
}

/// Squares strictly between sq1 and sq2, inclusive of sq2.
pub fn between(sq1: Square, sq2: Square) -> Bitboard {
    let (f1, r1) = (sq1.file().as_u8() as i32, sq1.rank().as_u8() as i32);
    let (f2, r2) = (sq2.file().as_u8() as i32, sq2.rank().as_u8() as i32);
    let (df, dr) = (f2 - f1, r2 - r1);

    // Must be aligned: same rank, file, or diagonal
    if dr != 0 && df != 0 && df.abs() != dr.abs() {
        return Bitboard::from_square(sq2);
    }

    let sf = df.signum();
    let sr = dr.signum();
    let mut result = Bitboard(0);
    let (mut f, mut r) = (f1 + sf, r1 + sr);

    while (0..=7).contains(&f) && (0..=7).contains(&r) {
        let sq = Square::new(File::from_u8(f as u8), Rank::from_u8(r as u8));
        result.set(sq.0);
        if f == f2 && r == r2 {
            break;
        }
        f += sf;
        r += sr;
    }
    result
}

pub fn check_mask(board: &Board, sq: Square, c: Color) -> (Bitboard, u32) {
    let opp = !c;
    let mut checks = 0u32;
    let mut mask = Bitboard(0);

    // Knight checks
    let knight_atk = attacks::knight(sq) & board.pieces_pt_color(PieceType::KNIGHT, opp);
    if knight_atk.0 != 0 {
        mask = mask | knight_atk;
        checks += 1;
    }

    // Pawn checks
    let pawn_atk = attacks::pawn(c, sq) & board.pieces_pt_color(PieceType::PAWN, opp);
    if pawn_atk.0 != 0 {
        mask = mask | pawn_atk;
        checks += 1;
    }

    // Bishop / diagonal queen checks
    let diag_pieces = board.pieces_pt_color(PieceType::BISHOP, opp)
        | board.pieces_pt_color(PieceType::QUEEN, opp);
    let bishop_atk = attacks::bishop(sq, board.occ()) & diag_pieces;
    if bishop_atk.0 != 0 {
        mask = mask | between(sq, Square(bishop_atk.lsb()));
        checks += 1;
    }

    // Rook / orthogonal queen checks
    let orth_pieces =
        board.pieces_pt_color(PieceType::ROOK, opp) | board.pieces_pt_color(PieceType::QUEEN, opp);
    let rook_atk = attacks::rook(sq, board.occ()) & orth_pieces;
    if rook_atk.0 != 0 {
        if rook_atk.count() > 1 {
            return (mask, 2);
        }
        mask = mask | between(sq, Square(rook_atk.lsb()));
        checks += 1;
    }

    if mask.0 == 0 {
        (DEFAULT_CHECKMASK, checks)
    } else {
        (mask, checks)
    }
}

/// Returns the pin mask for the given piece type (Rook = HV pins, Bishop = diagonal pins).
pub fn pin_mask(
    board: &Board,
    king_sq: Square,
    occ_opp: Bitboard,
    occ_us: Bitboard,
    is_rook: bool,
) -> Bitboard {
    let opp = !board.side_to_move();
    let sliders = if is_rook {
        board.pieces_pt_color(PieceType::ROOK, opp) | board.pieces_pt_color(PieceType::QUEEN, opp)
    } else {
        board.pieces_pt_color(PieceType::BISHOP, opp) | board.pieces_pt_color(PieceType::QUEEN, opp)
    } & board.us(opp);

    let king_attacks = if is_rook {
        attacks::rook(king_sq, occ_opp)
    } else {
        attacks::bishop(king_sq, occ_opp)
    } & sliders;

    let mut pin = Bitboard(0);
    let mut attackers = king_attacks;
    while attackers.0 != 0 {
        let ray = between(king_sq, Square(attackers.pop()));
        // Exactly one friendly piece on the ray => it's pinned
        if (ray & occ_us).count() == 1 {
            pin = pin | ray;
        }
    }
    pin
}

/// Returns squares attacked/seen by `c`'s pieces. Used to restrict king moves.
pub fn seen_squares(board: &Board, c: Color, enemy_empty: Bitboard) -> Bitboard {
    let king_sq = board.king_sq(!c);
    if (attacks::king(king_sq) & enemy_empty).0 == 0 && !board.chess960() {
        return Bitboard(0);
    }

    // Remove the enemy king so sliders can "see through" it
    let occ = board.occ() ^ Bitboard::from_square(king_sq);
    let queens = board.pieces_pt_color(PieceType::QUEEN, c);
    let pawns = board.pieces_pt_color(PieceType::PAWN, c);
    let mut knights = board.pieces_pt_color(PieceType::KNIGHT, c);
    let mut bishops = board.pieces_pt_color(PieceType::BISHOP, c) | queens;
    let mut rooks = board.pieces_pt_color(PieceType::ROOK, c) | queens;

    let mut seen = attacks::pawn_left_attacks(pawns, c) | attacks::pawn_right_attacks(pawns, c);
    while knights.0 != 0 {
        seen = seen | attacks::knight(Square(knights.pop()));
    }
    while bishops.0 != 0 {
        seen = seen | attacks::bishop(Square(bishops.pop()), occ);
    }
    while rooks.0 != 0 {
        seen = seen | attacks::rook(Square(rooks.pop()), occ);
    }
    seen | attacks::king(board.king_sq(c))
}

// Per-piece move generators

#[inline(always)]
pub fn generate_knight_moves(sq: Square) -> Bitboard {
    attacks::knight(sq)
}

#[inline(always)]
pub fn generate_bishop_moves(sq: Square, pin_d: Bitboard, occ_all: Bitboard) -> Bitboard {
    if (pin_d & Bitboard::from_square(sq)).0 != 0 {
        attacks::bishop(sq, occ_all) & pin_d
    } else {
        attacks::bishop(sq, occ_all)
    }
}

#[inline(always)]
pub fn generate_rook_moves(sq: Square, pin_hv: Bitboard, occ_all: Bitboard) -> Bitboard {
    if (pin_hv & Bitboard::from_square(sq)).0 != 0 {
        attacks::rook(sq, occ_all) & pin_hv
    } else {
        attacks::rook(sq, occ_all)
    }
}

#[inline(always)]
pub fn generate_queen_moves(
    sq: Square,
    pin_d: Bitboard,
    pin_hv: Bitboard,
    occ_all: Bitboard,
) -> Bitboard {
    let sq_bb = Bitboard::from_square(sq);
    if (pin_d & sq_bb).0 != 0 {
        attacks::bishop(sq, occ_all) & pin_d
    } else if (pin_hv & sq_bb).0 != 0 {
        attacks::rook(sq, occ_all) & pin_hv
    } else {
        attacks::rook(sq, occ_all) | attacks::bishop(sq, occ_all)
    }
}

#[inline(always)]
pub fn generate_king_moves(sq: Square, seen: Bitboard, movable_square: Bitboard) -> Bitboard {
    attacks::king(sq) & movable_square & !seen
}

// Castle moves

pub fn generate_castle_moves(
    board: &Board,
    sq: Square,
    seen: Bitboard,
    pin_hv: Bitboard,
    c: Color,
) -> Bitboard {
    if !Square::back_rank(sq, c) || !board.castling_rights().has_color(c) {
        return Bitboard(0);
    }

    let rights = board.castling_rights();
    let mut moves = Bitboard(0);

    for &is_king_side in &[true, false] {
        if !rights.has(c, is_king_side) {
            continue;
        }

        // No pieces on the castling path
        if (board.occ() & board.get_castling_path(c, is_king_side)).0 != 0 {
            continue;
        }

        // No attacks on the king path
        let king_to = Square::castling_king_square(is_king_side, c);
        if (between(sq, king_to) & seen).0 != 0 {
            continue;
        }

        // Chess960: rook pinned on back rank
        let rook_file = rights.get_rook_file(c, is_king_side);
        let from_rook_bb = Bitboard::from_square(Square::new(rook_file, sq.rank()));
        if board.chess960() && (pin_hv & board.us(c) & from_rook_bb).0 != 0 {
            continue;
        }

        moves = moves | from_rook_bb;
    }

    moves
}

// EP move generator

pub fn generate_ep_move(
    board: &Board,
    checkmask: Bitboard,
    pin_d: Bitboard,
    pawns_lr: Bitboard,
    ep: Square,
    c: Color,
) -> [Move; 2] {
    let mut moves = [Move::from_raw(Move::NO_MOVE), Move::from_raw(Move::NO_MOVE)];
    let mut i = 0usize;

    let down = Direction::make_direction(Direction::South, c);
    let ep_pawn_sq = ep + down;

    // Check checkmask validity
    let ep_bb = Bitboard::from_square(ep);
    let ep_pawn_bb = Bitboard::from_square(ep_pawn_sq);
    if (checkmask & (ep_pawn_bb | ep_bb)).0 == 0 {
        return moves;
    }

    let k_sq = board.king_sq(c);
    let king_mask = Bitboard::from_square(k_sq) & Bitboard(ep_pawn_sq.rank().bb());
    let enemy_queen_rook = (board.pieces_pt_color(PieceType::ROOK, !c)
        | board.pieces_pt_color(PieceType::QUEEN, !c))
        & board.us(!c);

    let mut ep_bb_from = attacks::pawn(!c, ep) & pawns_lr;

    while ep_bb_from.0 != 0 {
        let from = Square(ep_bb_from.pop());
        let to = ep;

        // Pinned pawn that can't go to ep square
        if (Bitboard::from_square(from) & pin_d).0 != 0
            && (pin_d & Bitboard::from_square(ep)).0 == 0
        {
            continue;
        }

        let connecting_pawns = Bitboard::from_square(ep_pawn_sq) | Bitboard::from_square(from);
        let is_possible_pin = king_mask.0 != 0 && enemy_queen_rook.0 != 0;

        if is_possible_pin
            && (attacks::rook(k_sq, board.occ() ^ connecting_pawns) & enemy_queen_rook).0 != 0
        {
            break;
        }

        moves[i] = Move::make::<{ Move::ENPASSANT }>(from, to, PieceType::KNIGHT);
        i += 1;
    }

    moves
}

// Pawn moves

pub fn generate_pawn_moves(
    board: &Board,
    moves: &mut Movelist,
    pin_d: Bitboard,
    pin_hv: Bitboard,
    checkmask: Bitboard,
    occ_opp: Bitboard,
    c: Color,
    mt: MoveGenType,
) {
    let up = Direction::make_direction(Direction::North, c);
    let down = Direction::make_direction(Direction::South, c);
    let down_left = Direction::make_direction(Direction::SouthWest, c);
    let down_right = Direction::make_direction(Direction::SouthEast, c);
    let up_left = Direction::make_direction(Direction::NorthWest, c);
    let up_right = Direction::make_direction(Direction::NorthEast, c);

    let rank_b_promo = Bitboard(Rank::relative(Rank::Rank7, c).bb());
    let rank_promo = Bitboard(Rank::relative(Rank::Rank8, c).bb());
    let double_push_rank = Bitboard(Rank::relative(Rank::Rank3, c).bb());

    let pawns = board.pieces_pt_color(PieceType::PAWN, c);

    // Pawns that can take left or right (not pinned HV)
    let pawns_lr = pawns & !pin_hv;
    let unpinned_pawns_lr = pawns_lr & !pin_d;
    let pinned_pawns_lr = pawns_lr & pin_d;

    let l_shift_unpinned = attacks::shift(unpinned_pawns_lr, up_left);
    let l_shift_pinned = attacks::shift(pinned_pawns_lr, up_left) & pin_d;
    let mut l_pawns = (l_shift_unpinned | l_shift_pinned) & occ_opp & checkmask;

    let r_shift_unpinned = attacks::shift(unpinned_pawns_lr, up_right);
    let r_shift_pinned = attacks::shift(pinned_pawns_lr, up_right) & pin_d;
    let mut r_pawns = (r_shift_unpinned | r_shift_pinned) & occ_opp & checkmask;

    // Forward pushes
    let pawns_hv = pawns & !pin_d;
    let pawns_pinned_hv = pawns_hv & pin_hv;
    let pawns_unpinned_hv = pawns_hv & !pin_hv;

    let single_push_unpinned = attacks::shift(pawns_unpinned_hv, up) & !board.occ();
    let single_push_pinned = attacks::shift(pawns_pinned_hv, up) & pin_hv & !board.occ();

    let mut single_push = (single_push_unpinned | single_push_pinned) & checkmask;

    let dp_from_unpinned =
        attacks::shift(single_push_unpinned & double_push_rank, up) & !board.occ();
    let dp_from_pinned = attacks::shift(single_push_pinned & double_push_rank, up) & !board.occ();
    let mut double_push = (dp_from_unpinned | dp_from_pinned) & checkmask;

    // Promotion captures
    if (pawns & rank_b_promo).0 != 0 {
        let mut promo_left = l_pawns & rank_promo;
        let mut promo_right = r_pawns & rank_promo;
        let mut promo_push = single_push & rank_promo;

        if mt != MoveGenType::Quiet {
            while promo_left.0 != 0 {
                let index = Square(promo_left.pop());
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_right,
                    index,
                    PieceType::QUEEN,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_right,
                    index,
                    PieceType::ROOK,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_right,
                    index,
                    PieceType::BISHOP,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_right,
                    index,
                    PieceType::KNIGHT,
                ));
            }
            while promo_right.0 != 0 {
                let index = Square(promo_right.pop());
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_left,
                    index,
                    PieceType::QUEEN,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_left,
                    index,
                    PieceType::ROOK,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_left,
                    index,
                    PieceType::BISHOP,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down_left,
                    index,
                    PieceType::KNIGHT,
                ));
            }
        }

        if mt != MoveGenType::Capture {
            while promo_push.0 != 0 {
                let index = Square(promo_push.pop());
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down,
                    index,
                    PieceType::QUEEN,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down,
                    index,
                    PieceType::ROOK,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down,
                    index,
                    PieceType::BISHOP,
                ));
                moves.add(Move::make::<{ Move::PROMOTION }>(
                    index + down,
                    index,
                    PieceType::KNIGHT,
                ));
            }
        }
    }

    // Remove promotions from regular moves
    single_push = single_push & !rank_promo;
    l_pawns = l_pawns & !rank_promo;
    r_pawns = r_pawns & !rank_promo;

    if mt != MoveGenType::Quiet {
        while l_pawns.0 != 0 {
            let index = Square(l_pawns.pop());
            moves.add(Move::make::<{ Move::NORMAL }>(
                index + down_right,
                index,
                PieceType::KNIGHT,
            ));
        }
        while r_pawns.0 != 0 {
            let index = Square(r_pawns.pop());
            moves.add(Move::make::<{ Move::NORMAL }>(
                index + down_left,
                index,
                PieceType::KNIGHT,
            ));
        }
    }

    if mt != MoveGenType::Capture {
        while single_push.0 != 0 {
            let index = Square(single_push.pop());
            moves.add(Move::make::<{ Move::NORMAL }>(
                index + down,
                index,
                PieceType::KNIGHT,
            ));
        }
        while double_push.0 != 0 {
            let index = Square(double_push.pop());
            moves.add(Move::make::<{ Move::NORMAL }>(
                index + down + down,
                index,
                PieceType::KNIGHT,
            ));
        }
    }

    // En passant
    if mt == MoveGenType::Quiet {
        return;
    }

    let ep = board.enpassant_sq();
    if ep != Square::NO_SQ {
        let ep_moves = generate_ep_move(board, checkmask, pin_d, pawns_lr, ep, c);
        for m in &ep_moves {
            if m.raw() != Move::NO_MOVE {
                moves.add(*m);
            }
        }
    }
}

/// Add all moves from `from` to each set bit in `targets`.
fn add_moves(movelist: &mut Movelist, from: Square, mut targets: Bitboard) {
    while targets.0 != 0 {
        let to = Square(targets.pop());
        movelist.add(Move::make::<{ Move::NORMAL }>(from, to, PieceType::KNIGHT));
    }
}

/// Iterate pieces and add normal moves via a closure returning target squares.
fn generate_piece_moves(
    movelist: &mut Movelist,
    mut pieces: Bitboard,
    targets: impl Fn(Square) -> Bitboard,
) {
    while pieces.0 != 0 {
        let from = Square(pieces.pop());
        add_moves(movelist, from, targets(from));
    }
}

// legalmoves (color-specific inner)

fn legalmoves_colored(
    movelist: &mut Movelist,
    board: &Board,
    pieces: u32,
    c: Color,
    mt: MoveGenType,
) {
    let king_sq = board.king_sq(c);

    let occ_us = board.us(c);
    let occ_opp = board.us(!c);
    let occ_all = occ_us | occ_opp;
    let opp_empty = !occ_us;

    let (checkmask, checks) = check_mask(board, king_sq, c);
    let pin_hv = pin_mask(board, king_sq, occ_opp, occ_us, true);
    let pin_d = pin_mask(board, king_sq, occ_opp, occ_us, false);

    debug_assert!(checks <= 2);

    let movable_square = match mt {
        MoveGenType::All => opp_empty,
        MoveGenType::Capture => occ_opp,
        MoveGenType::Quiet => !occ_all,
    };

    if pieces & PieceGenType::KING != 0 {
        let seen = seen_squares(board, !c, opp_empty);

        generate_piece_moves(movelist, Bitboard::from_square(king_sq), |sq| {
            generate_king_moves(sq, seen, movable_square)
        });

        if mt != MoveGenType::Capture && checks == 0 {
            let mut moves_bb = generate_castle_moves(board, king_sq, seen, pin_hv, c);
            while moves_bb.0 != 0 {
                let to = Square(moves_bb.pop());
                movelist.add(Move::make::<{ Move::CASTLING }>(
                    king_sq,
                    to,
                    PieceType::KNIGHT,
                ));
            }
        }
    }

    // Double check: only king moves are legal
    if checks == 2 {
        return;
    }

    let movable_square = movable_square & checkmask;

    if pieces & PieceGenType::PAWN != 0 {
        generate_pawn_moves(board, movelist, pin_d, pin_hv, checkmask, occ_opp, c, mt);
    }

    if pieces & PieceGenType::KNIGHT != 0 {
        let knights_mask = board.pieces_pt_color(PieceType::KNIGHT, c) & !(pin_d | pin_hv);
        generate_piece_moves(movelist, knights_mask, |sq| {
            generate_knight_moves(sq) & movable_square
        });
    }

    if pieces & PieceGenType::BISHOP != 0 {
        let bishops_mask = board.pieces_pt_color(PieceType::BISHOP, c) & !pin_hv;
        generate_piece_moves(movelist, bishops_mask, |sq| {
            generate_bishop_moves(sq, pin_d, occ_all) & movable_square
        });
    }

    if pieces & PieceGenType::ROOK != 0 {
        let rooks_mask = board.pieces_pt_color(PieceType::ROOK, c) & !pin_d;
        generate_piece_moves(movelist, rooks_mask, |sq| {
            generate_rook_moves(sq, pin_hv, occ_all) & movable_square
        });
    }

    if pieces & PieceGenType::QUEEN != 0 {
        let queens_mask = board.pieces_pt_color(PieceType::QUEEN, c) & !(pin_d & pin_hv);
        generate_piece_moves(movelist, queens_mask, |sq| {
            generate_queen_moves(sq, pin_d, pin_hv, occ_all) & movable_square
        });
    }
}

/// Generate all legal moves for the given board.
pub fn legalmoves(movelist: &mut Movelist, board: &Board, mt: MoveGenType, pieces: u32) {
    movelist.clear();

    if board.side_to_move() == Color::White {
        legalmoves_colored(movelist, board, pieces, Color::White, mt);
    } else {
        legalmoves_colored(movelist, board, pieces, Color::Black, mt);
    }
}

/// Check whether the en passant square is valid (can actually be captured).
pub fn is_ep_square_valid(board: &Board, ep: Square) -> bool {
    let stm = board.side_to_move();

    let occ_us = board.us(stm);
    let occ_opp = board.us(!stm);
    let king_sq = board.king_sq(stm);

    let (checkmask, _) = check_mask(board, king_sq, stm);
    let pin_hv = pin_mask(board, king_sq, occ_opp, occ_us, true);
    let pin_d = pin_mask(board, king_sq, occ_opp, occ_us, false);

    let pawns = board.pieces_pt_color(PieceType::PAWN, stm);
    let pawns_lr = pawns & !pin_hv;

    let m = generate_ep_move(board, checkmask, pin_d, pawns_lr, ep, stm);
    m.iter().any(|mv| mv.raw() != Move::NO_MOVE)
}
