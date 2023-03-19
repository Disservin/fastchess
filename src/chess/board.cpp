#define NOMINMAX

#include "chess/board.hpp"

#include <algorithm>
#include <bitset>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "chess/attacks.hpp"
#include "chess/movegen.hpp"
#include "chess/zobrist.hpp"
#include "options.hpp"

namespace fast_chess
{
void Board::loadFen(const std::string &fen)
{
    for (const auto c : {WHITE, BLACK})
    {
        for (PieceType p = PAWN; p < NONETYPE; p++)
        {
            pieceBB_[c][p] = 0ULL;
        }
    }

    const std::vector<std::string> params = CMD::splitString(fen, ' ');

    const std::string position = params[0];
    const std::string move_right = params[1];
    const std::string castling = params[2];
    const std::string en_passant = params[3];

    half_move_clock_ = std::stoi(params.size() > 4 ? params[4] : "0");
    full_move_number_ = std::stoi(params.size() > 4 ? params[5] : "1") * 2;

    side_to_move_ = (move_right == "w") ? WHITE : BLACK;

    board_.reset(NONE);

    Square square = Square(56);
    for (int index = 0; index < static_cast<int>(position.size()); index++)
    {
        char curr = position[index];
        if (charToPiece.find(curr) != charToPiece.end())
        {
            const Piece piece = charToPiece[curr];
            placePiece(piece, square);

            square = Square(square + 1);
        }
        else if (curr == '/')
            square = Square(square - 16);
        else if (isdigit(curr))
        {
            square = Square(square + (curr - '0'));
        }
    }

    castling_rights_ = 0;

    for (std::size_t i = 0; i < castling.size(); i++)
    {
        if (readCastleString.find(castling[i]) != readCastleString.end())
            castling_rights_ |= readCastleString[castling[i]];
    }

    if (en_passant == "-")
    {
        enpassant_square_ = NO_SQ;
    }
    else
    {
        const char letter = en_passant[0];
        const int file = letter - 96;
        const int rank = en_passant[1] - 48;
        enpassant_square_ = Square((rank - 1) * 8 + file - 1);
    }

    hash_key_ = zobristHash();
}

std::string Board::getFen() const
{
    std::stringstream ss;

    // Loop through the ranks of the board in reverse order
    for (int rank = 7; rank >= 0; rank--)
    {
        int free_space = 0;

        // Loop through the files of the board
        for (int file = 0; file < 8; file++)
        {
            // Calculate the square index
            const int sq = rank * 8 + file;

            // Get the piece at the current square
            const Piece piece = pieceAt(Square(sq));

            // If there is a piece at the current square
            if (piece != NONE)
            {
                // If there were any empty squares before this piece,
                // append the number of empty squares to the FEN string
                if (free_space)
                {
                    ss << free_space;
                    free_space = 0;
                }

                // Append the character representing the piece to the FEN string
                ss << pieceToChar[piece];
            }
            else
            {
                // If there is no piece at the current square, increment the
                // counter for the number of empty squares
                free_space++;
            }
        }

        // If there are any empty squares at the end of the rank,
        // append the number of empty squares to the FEN string
        if (free_space != 0)
        {
            ss << free_space;
        }

        // Append a "/" character to the FEN string, unless this is the last rank
        ss << (rank > 0 ? "/" : "");
    }

    // Append " w " or " b " to the FEN string, depending on which player's turn it is
    ss << (side_to_move_ == WHITE ? " w " : " b ");

    // Append the appropriate characters to the FEN string to indicate
    // whether or not castling is allowed for each player
    if (castling_rights_ & WK)
        ss << "K";
    if (castling_rights_ & WQ)
        ss << "Q";
    if (castling_rights_ & BK)
        ss << "k";
    if (castling_rights_ & BQ)
        ss << "q";
    if (castling_rights_ == 0)
        ss << "-";

    // Append information about the en passant square (if any)
    // and the halfmove clock and fullmove number to the FEN string
    if (enpassant_square_ == NO_SQ)
        ss << " - ";
    else
        ss << " " << squareToString[enpassant_square_] << " ";

    ss << int(half_move_clock_) << " " << int(full_move_number_ / 2);

    // Return the resulting FEN string
    return ss.str();
}

bool Board::makeMove(Move move)
{
    Movelist moves;
    Movegen::legalmoves(*this, moves);

    if (moves.find(move) == -1)
        return false;

    const Square from_sq = move.from_sq;
    const Square to_sq = move.to_sq;
    const Piece p = pieceAt(from_sq);
    const PieceType pt = move.moving_piece;
    const Piece capture = pieceAt(move.to_sq);

    assert(from_sq >= 0 && from_sq < 64);
    assert(to_sq >= 0 && to_sq < 64);
    assert(typeOfPiece(capture) != KING);
    if (p == NONE)
    {
        std::cout << *this << std::endl;
        std::cout << uciMove(move) << std::endl;
    }
    assert(p != NONE);

    // *****************************
    // STORE STATE HISTORY
    // *****************************

    prev_boards_.emplace_back(enpassant_square_, castling_rights_, half_move_clock_, capture);
    hash_history_.emplace_back(hash_key_);

    half_move_clock_++;
    full_move_number_++;

    const bool ep = to_sq == enpassant_square_;
    const bool isCastling =
        pt == KING && typeOfPiece(capture) == ROOK && colorOf(from_sq) == colorOf(to_sq);

    if (enpassant_square_ != NO_SQ)
        hash_key_ ^= updateKeyEnPassant(enpassant_square_);

    hash_key_ ^= updateKeyCastling();
    enpassant_square_ = NO_SQ;

    if (pt == KING)
    {
        removecastling_rightsAll(side_to_move_);

        if (isCastling)
        {
            const Piece rook = side_to_move_ == WHITE ? WHITEROOK : BLACKROOK;
            Square rookToSq =
                fileRankSquare(to_sq > from_sq ? FILE_F : FILE_D, squareRank(from_sq));
            Square kingToSq =
                fileRankSquare(to_sq > from_sq ? FILE_G : FILE_C, squareRank(from_sq));
            removePiece(p, from_sq);
            removePiece(rook, to_sq);

            placePiece(p, kingToSq);
            placePiece(rook, rookToSq);

            side_to_move_ = ~side_to_move_;

            hash_key_ ^= updateKeyside_to_move();
            hash_key_ ^= updateKeyCastling();

            return true;
        }
    }
    else if (pt == ROOK)
    {
        removecastling_rightsRook(from_sq);
    }
    else if (pt == PAWN)
    {
        half_move_clock_ = 0;
        if (ep)
        {
            removePiece(make_piece(PAWN, ~side_to_move_), Square(to_sq ^ 8));
        }
        else if (std::abs(from_sq - to_sq) == 16)
        {
            Bitboard epMask = PawnAttacks(Square(to_sq ^ 8), side_to_move_);
            if (epMask & pieces(PAWN, ~side_to_move_))
            {
                enpassant_square_ = Square(to_sq ^ 8);
                hash_key_ ^= updateKeyEnPassant(enpassant_square_);

                assert(pieceAt(enpassant_square_) == NONE);
            }
        }
    }

    if (capture != NONE)
    {
        half_move_clock_ = 0;
        if (typeOfPiece(capture) == ROOK)
            removecastling_rightsRook(to_sq);

        removePiece(capture, to_sq);
    }

    if (move.promotion_piece != NONETYPE)
    {
        half_move_clock_ = 0;
        removePiece(make_piece(PAWN, side_to_move_), from_sq);
        placePiece(make_piece(move.promotion_piece, side_to_move_), to_sq);
    }
    else
    {
        assert(pieceAt(to_sq) == NONE);

        removePiece(p, from_sq);
        placePiece(p, to_sq);
    }

    hash_key_ ^= updateKeyside_to_move();
    hash_key_ ^= updateKeyCastling();

    side_to_move_ = ~side_to_move_;

    return true;
}

void Board::unmakeMove(Move move)
{
    const auto restore = prev_boards_.back();
    prev_boards_.pop_back();

    hash_key_ = hash_history_.back();
    hash_history_.pop_back();

    enpassant_square_ = restore.enpassant;
    castling_rights_ = restore.castling;
    half_move_clock_ = restore.halfMove;

    full_move_number_--;

    const Square from_sq = move.from_sq;
    const Square to_sq = move.to_sq;
    const Piece capture = restore.captured_piece;
    const PieceType pt = move.moving_piece;

    side_to_move_ = ~side_to_move_;

    const Piece p = make_piece(pt, side_to_move_);

    const bool promotion = move.promotion_piece != NONETYPE;

    if ((p == WHITEKING && capture == WHITEROOK) || (p == BLACKKING && capture == BLACKROOK))
    {
        Square rookToSq = to_sq;
        Piece rook = side_to_move_ == WHITE ? WHITEROOK : BLACKROOK;
        Square rookFromSq = fileRankSquare(to_sq > from_sq ? FILE_F : FILE_D, squareRank(from_sq));
        Square KingToSq = fileRankSquare(to_sq > from_sq ? FILE_G : FILE_C, squareRank(from_sq));

        // We need to remove both pieces first and then place them back.
        removePiece(rook, rookFromSq);
        removePiece(p, KingToSq);

        placePiece(p, from_sq);
        placePiece(rook, rookToSq);

        return;
    }
    else if (promotion)
    {
        removePiece(pieceAt(to_sq), to_sq);
        placePiece(make_piece(PAWN, side_to_move_), from_sq);
        if (capture != NONE)
            placePiece(capture, to_sq);

        return;
    }
    else
    {
        removePiece(p, to_sq);
        placePiece(p, from_sq);
    }

    if (to_sq == enpassant_square_ && pt == PAWN)
    {
        placePiece(make_piece(PAWN, ~side_to_move_), Square(enpassant_square_ ^ 8));
    }
    else if (capture != NONE)
    {
        placePiece(capture, to_sq);
    }
}

Bitboard Board::us(Color c) const
{
    return pieceBB_[c][PAWN] | pieceBB_[c][KNIGHT] | pieceBB_[c][BISHOP] | pieceBB_[c][ROOK] |
           pieceBB_[c][QUEEN] | pieceBB_[c][KING];
}

Bitboard Board::allBB() const
{
    return us(WHITE) | us(BLACK);
}

Square Board::KingSQ(Color c) const
{
    return lsb(pieces(KING, c));
}

uint64_t Board::getHash() const
{
    return hash_key_;
}

Bitboard Board::pieces(PieceType type, Color color) const
{
    return pieceBB_[color][type];
}

Piece Board::pieceAt(Square square) const
{
    return board_[square];
}

uint64_t Board::zobristHash() const
{
    uint64_t hash = 0ULL;
    uint64_t wPieces = us(WHITE);
    uint64_t bPieces = us(BLACK);
    // Piece hashes
    while (wPieces)
    {
        const Square sq = poplsb(wPieces);
        hash ^= updateKeyPiece(pieceAt(sq), sq);
    }
    while (bPieces)
    {
        const Square sq = poplsb(bPieces);
        hash ^= updateKeyPiece(pieceAt(sq), sq);
    }
    // Ep hash
    uint64_t ep_hash = 0ULL;
    if (enpassant_square_ != NO_SQ)
    {
        ep_hash = updateKeyEnPassant(enpassant_square_);
    }
    // Turn hash
    const uint64_t turn_hash = side_to_move_ == WHITE ? RANDOM_ARRAY[780] : 0;
    // Castle hash
    const uint64_t cast_hash = updateKeyCastling();

    return hash ^ cast_hash ^ turn_hash ^ ep_hash;
}

Color Board::getSideToMove() const
{
    return side_to_move_;
}

Square Board::getEnpassantSquare() const
{
    return enpassant_square_;
}

uint8_t Board::getCastlingRights() const
{
    return castling_rights_;
}

Bitboard Board::getSquaresBetweenBB(int sq_1, int sq_2) const
{
    return squares_between_bb_[sq_1][sq_2];
}

bool Board::isChess960() const
{
    return chess960_;
}

bool Board::isRepetition() const
{
    uint8_t c = 0;

    for (int i = static_cast<int>(hash_history_.size()) - 2;
         i >= 0 && i >= static_cast<int>(hash_history_.size()) - half_move_clock_ - 1; i -= 2)
    {
        if (hash_history_[i] == hash_key_)
            c++;

        if (c == 2)
            return true;
    }

    return false;
}

Bitboard Board::attacksByPiece(PieceType pt, Square sq, Color c, Bitboard occ) const
{
    switch (pt)
    {
    case PAWN:
        return PawnAttacks(sq, c);
    case KNIGHT:
        return KnightAttacks(sq);
    case BISHOP:
        return BishopAttacks(sq, occ);
    case ROOK:
        return RookAttacks(sq, occ);
    case QUEEN:
        return QueenAttacks(sq, occ);
    case KING:
        return KingAttacks(sq);
    case NONETYPE:
        return 0ULL;
    default:
        return 0ULL;
    }
}

GameResult Board::isGameOver()
{
    if (half_move_clock_ >= 100)
    {
        if (isSquareAttacked(~side_to_move_, lsb(pieces(KING, side_to_move_))) &&
            !Movegen::hasLegalMoves(*this))
            return GameResult::LOSE;
        return GameResult::DRAW;
    }

    const auto count = popcount(allBB());

    if (count == 2)
        return GameResult::DRAW;

    if (count == 3)
    {
        if (pieces<BISHOP, WHITE>() || pieces<BISHOP, BLACK>())
            return GameResult::DRAW;
        if (pieces<KNIGHT, WHITE>() || pieces<KNIGHT, BLACK>())
            return GameResult::DRAW;
    }

    if (count == 4)
    {
        if (pieces<BISHOP, WHITE>() && pieces<BISHOP, BLACK>() &&
            sameColor(lsb(pieces<BISHOP, WHITE>()), lsb(pieces<BISHOP, BLACK>())))
            return GameResult::DRAW;
    }

    if (isRepetition())
        return GameResult::DRAW;

    if (!Movegen::hasLegalMoves(*this))
    {
        if (isSquareAttacked(~side_to_move_, lsb(pieces(KING, side_to_move_))))
            return GameResult::LOSE;
        return GameResult::DRAW;
    }

    return GameResult::NONE;
}

void Board::removecastling_rightsAll(Color c)
{
    if (c == WHITE)
    {
        castling_rights_ &= ~(WK | WQ);
    }
    else
    {
        castling_rights_ &= ~(BK | BQ);
    }
}

void Board::removecastling_rightsRook(Square sq)
{

    if (castlingMapRook.find(sq) != castlingMapRook.end())
    {
        castling_rights_ &= ~castlingMapRook[sq];
    }
}

void Board::initializeLookupTables()
{
    // initialize squares between table

    Bitboard sqs;
    for (Square sq1 = SQ_A1; sq1 <= SQ_H8; ++sq1)
    {
        for (Square sq2 = SQ_A1; sq2 <= SQ_H8; ++sq2)
        {
            sqs = (1ULL << sq1) | (1ULL << sq2);
            if (sq1 == sq2)
                squares_between_bb_[sq1][sq2] = 0ull;
            else if (squareFile(sq1) == squareFile(sq2) || squareRank(sq1) == squareRank(sq2))
                squares_between_bb_[sq1][sq2] = RookAttacks(sq1, sqs) & RookAttacks(sq2, sqs);
            else if (diagonalOf(sq1) == diagonalOf(sq2) ||
                     anti_diagonalOf(sq1) == anti_diagonalOf(sq2))
                squares_between_bb_[sq1][sq2] = BishopAttacks(sq1, sqs) & BishopAttacks(sq2, sqs);
        }
    }
}

bool Board::isSquareAttacked(Color c, Square sq) const
{
    const Bitboard all = allBB();

    if (pieces(PAWN, c) & PawnAttacks(sq, ~c))
        return true;
    if (pieces(KNIGHT, c) & KnightAttacks(sq))
        return true;
    if ((pieces(BISHOP, c) | pieces(QUEEN, c)) & BishopAttacks(sq, all))
        return true;
    if ((pieces(ROOK, c) | pieces(QUEEN, c)) & RookAttacks(sq, all))
        return true;
    if (pieces(KING, c) & KingAttacks(sq))
        return true;
    return false;
}

PieceType Board::typeOfPiece(const Piece piece)
{
    constexpr PieceType PieceToPieceType[13] = {PAWN,   KNIGHT, BISHOP, ROOK,  QUEEN, KING,    PAWN,
                                                KNIGHT, BISHOP, ROOK,   QUEEN, KING,  NONETYPE};
    return PieceToPieceType[piece];
}

File Board::squareFile(Square sq)
{
    return File(sq & 7);
}

Rank Board::squareRank(Square sq)
{
    return Rank(sq >> 3);
}

Square Board::fileRankSquare(File f, Rank r)
{
    return Square((r << 3) + f);
}

Piece Board::make_piece(PieceType type, Color c)
{
    if (type == NONETYPE)
        return NONE;
    return Piece(type + 6 * c);
}

// returns diagonal of given square
uint8_t Board::diagonalOf(Square sq)
{
    return 7 + squareRank(sq) - squareFile(sq);
}

// returns anti diagonal of given square
uint8_t Board::anti_diagonalOf(Square sq)
{
    return squareRank(sq) + squareFile(sq);
}

bool Board::sameColor(int sq1, int sq2)
{
    return ((9 * (sq1 ^ sq2)) & 8) == 0;
}

Color Board::colorOf(Piece p)
{
    if (p == NONE)
        return NO_COLOR;
    return Color(p / 6);
}

Color Board::colorOf(Square loc) const
{
    return Color((pieceAt(loc) / 6));
}

uint8_t Board::squareDistance(Square a, Square b)
{
    return std::max(std::abs(squareFile(a) - squareFile(b)),
                    std::abs(squareRank(a) - squareRank(b)));
}

void Board::placePiece(Piece piece, Square sq)
{
    hash_key_ ^= updateKeyPiece(piece, sq);
    pieceBB_[colorOf(piece)][typeOfPiece(piece)] |= (1ULL << sq);
    board_[sq] = piece;
}

void Board::removePiece(Piece piece, Square sq)
{
    hash_key_ ^= updateKeyPiece(piece, sq);
    pieceBB_[colorOf(piece)][typeOfPiece(piece)] &= ~(1ULL << sq);
    board_[sq] = NONE;
}

uint64_t Board::updateKeyPiece(Piece piece, Square sq) const
{
    return RANDOM_ARRAY[64 * hash_piece[piece] + sq];
}

uint64_t Board::updateKeyEnPassant(Square sq) const
{
    return RANDOM_ARRAY[772 + squareFile(sq)];
}

uint64_t Board::updateKeyCastling() const
{
    return castlingKey[castling_rights_];
}

uint64_t Board::updateKeyside_to_move() const
{
    return RANDOM_ARRAY[780];
}

Board::Board()
{
    initializeLookupTables();
}

Board::Board(const std::string &fen)
{
    initializeLookupTables();
    loadFen(fen);
}

std::ostream &operator<<(std::ostream &os, const Board &b)
{
    for (int i = 63; i >= 0; i -= 8)
    {
        os << " " << pieceToChar[b.board_[i - 7]] << " " << pieceToChar[b.board_[i - 6]] << " "
           << pieceToChar[b.board_[i - 5]] << " " << pieceToChar[b.board_[i - 4]] << " "
           << pieceToChar[b.board_[i - 3]] << " " << pieceToChar[b.board_[i - 2]] << " "
           << pieceToChar[b.board_[i - 1]] << " " << pieceToChar[b.board_[i]] << " \n";
    }
    os << "\n\n";
    os << "Side to move: " << static_cast<int>(b.side_to_move_) << "\n";
    os << "Castling rights: " << static_cast<int>(b.castling_rights_) << "\n";
    os << "Halfmoves: " << static_cast<int>(b.half_move_clock_) << "\n";
    os << "Fullmoves: " << static_cast<int>(b.full_move_number_) / 2 << "\n";
    os << "EP: " << static_cast<int>(b.enpassant_square_) << "\n";

    os << std::endl;
    return os;
}

void printBitboard(Bitboard bb)
{
    std::bitset<64> b(bb);
    std::string str_bitset = b.to_string();
    for (int i = 0; i < N_SQ; i += 8)
    {
        std::string x = str_bitset.substr(i, 8);
        reverse(x.begin(), x.end());
        std::cout << x << std::endl;
    }
    std::cout << '\n' << std::endl;
}

std::string uciMove(Move move)
{
    std::stringstream ss;

    // Add the from and to squares to the string stream
    if (move.moving_piece == KING && Board::squareDistance(move.to_sq, move.from_sq) >= 2)
    {
        move.to_sq = Board::fileRankSquare(move.to_sq > move.from_sq ? FILE_G : FILE_C,
                                           Board::squareRank(move.from_sq));
    }

    ss << squareToString[move.from_sq];
    ss << squareToString[move.to_sq];

    // If the move is a promotion, add the promoted piece to the string stream
    if (move.promotion_piece != NONETYPE)
    {
        ss << PieceTypeToPromPiece[move.promotion_piece];
    }

    return ss.str();
}

Square extractSquare(std::string_view squareStr)
{
    const char letter = squareStr[0];
    const int file = letter - 96;
    const int rank = squareStr[1] - 48;
    const int index = (rank - 1) * 8 + file - 1;
    return Square(index);
}

Move convertUciToMove(const Board &board, const std::string &input)
{
    const Square source = extractSquare(input.substr(0, 2));
    Square target = extractSquare(input.substr(2, 2));

    if (!board.isChess960() && Board::typeOfPiece(board.pieceAt(source)) == KING &&
        Board::squareDistance(target, source) == 2)
    {
        target =
            Board::fileRankSquare(target > source ? FILE_H : FILE_A, Board::squareRank(source));
    }

    switch (input.length())
    {
    case 4:
        return Move(source, target, Board::typeOfPiece(board.pieceAt(source)), NONETYPE);
    case 5:
        return Move(source, target, Board::typeOfPiece(board.pieceAt(source)),
                    charToPieceType[input.at(4)]);
    default:
        throw std::runtime_error("Warning: Cant parse move" + input);
        return Move(NO_SQ, NO_SQ, NONETYPE, NONETYPE);
    }
}

std::string MoveToRep(Board &b, Move move, bool isLan)
{
    static const std::string repPieceType[] = {"", "N", "B", "R", "Q", "K"};
    static const std::string repFile[] = {"a", "b", "c", "d", "e", "f", "g", "h"};

    const PieceType pt = move.moving_piece;

    assert(b.pieceAt(move.from_sq) != NONE);
    assert(pt == Board::typeOfPiece(b.pieceAt(move.from_sq)));

    if ((Board::make_piece(pt, b.getSideToMove()) == WHITEKING &&
         b.pieceAt(move.to_sq) == WHITEROOK) ||
        (Board::make_piece(pt, b.getSideToMove()) == BLACKKING &&
         b.pieceAt(move.to_sq) == BLACKROOK))
    {
        if (Board::squareFile(move.to_sq) < Board::squareFile(move.from_sq))
            return "O-O-O";
        else
            return "O-O";
    }

    std::string rep;
    if (pt != PAWN)
        rep = repPieceType[pt];

    Movelist moves;

    if (isLan)
    {
        // ambiguous move
        rep += repFile[Board::squareFile(move.from_sq)];
        rep += std::to_string(Board::squareRank(move.from_sq) + 1);

        // capture
        if (b.pieceAt(move.to_sq) != NONE || (pt == PAWN && b.getEnpassantSquare() == move.to_sq))
            rep += "x";
    }
    else
    {
        // ambiguous move
        Movegen::legalmoves(b, moves);

        for (const auto &cand : moves)
        {
            if (pt != PAWN && move != cand && Board::typeOfPiece(b.pieceAt(cand.from_sq)) == pt &&
                move.to_sq == cand.to_sq)
            {
                if (Board::squareFile(move.from_sq) == Board::squareFile(cand.from_sq))
                    rep += std::to_string(Board::squareRank(move.from_sq) + 1);
                else
                    rep += repFile[Board::squareFile(move.from_sq)];
                break;
            }
        }

        // capture
        if (b.pieceAt(move.to_sq) != NONE || (pt == PAWN && b.getEnpassantSquare() == move.to_sq))
            rep += (pt == PAWN ? repFile[Board::squareFile(move.from_sq)] : "") + "x";
    }

    rep += repFile[Board::squareFile(move.to_sq)];
    rep += std::to_string(Board::squareRank(move.to_sq) + 1);

    if (move.promotion_piece != NONETYPE)
        rep += "=" + repPieceType[move.promotion_piece];

    b.makeMove(move);

    moves.size = 0;
    Movegen::legalmoves(b, moves);

    const bool inCheck =
        b.isSquareAttacked(~b.getSideToMove(), lsb(b.pieces<KING>(b.getSideToMove())));

    b.unmakeMove(move);

    if (moves.size == 0 && inCheck)
        return rep + "#";

    if (inCheck)
        return rep + "+";

    return rep;
}

std::string resultToString(const MatchData &match)
{
    if (match.players.first.score == GameResult::WIN)
        return "1-0";
    else if (match.players.first.score == GameResult::LOSE)
        return "0-1";
    else if (match.players.first.score == GameResult::DRAW)
        return "1/2-1/2";
    else
        return "*";
}

} // namespace fast_chess
