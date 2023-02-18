#include <bitset>
#include <iostream>
#include <sstream>
#include <vector>

#include "attacks.h"
#include "board.h"

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

void Board::remove_piece(Piece piece, Square sq)
{
    pieceBB[colorOf(piece)][type_of_piece(piece)] &= ~(1ULL << sq);
    board[sq] = NONE;
}

void Board::place_piece(Piece piece, Square sq)
{
    pieceBB[colorOf(piece)][type_of_piece(piece)] |= (1ULL << sq);
    board[sq] = piece;
}

bool Board::isKingAttacked(Color c, Square sq)
{
    Bitboard all = allBB();

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

void Board::load_fen(const std::string &fen)
{
    for (auto c : {WHITE, BLACK})
    {
        for (PieceType p = PAWN; p < NONETYPE; p++)
        {
            pieceBB[c][p] = 0ULL;
        }
    }

    std::vector<std::string> params = splitString(fen, ' ');

    const std::string position = params[0];
    const std::string move_right = params[1];
    const std::string castling = params[2];
    const std::string en_passant = params[3];

    const std::string half_move_clock = params.size() > 4 ? params[4] : "0";
    const std::string full_move_counter = params.size() > 4 ? params[5] : "1";

    sideToMove = (move_right == "w") ? WHITE : BLACK;

    board.reset(NONE);

    Square square = Square(56);
    for (int index = 0; index < static_cast<int>(position.size()); index++)
    {
        char curr = position[index];
        if (charToPiece.find(curr) != charToPiece.end())
        {
            const Piece piece = charToPiece[curr];
            place_piece(piece, square);

            square = Square(square + 1);
        }
        else if (curr == '/')
            square = Square(square - 16);
        else if (isdigit(curr))
        {
            square = Square(square + (curr - '0'));
        }
    }

    castlingRights = 0;

    for (size_t i = 0; i < castling.size(); i++)
    {
        if (readCastleString.find(castling[i]) != readCastleString.end())
            castlingRights |= readCastleString[castling[i]];
    }

    if (en_passant == "-")
    {
        enPassantSquare = NO_SQ;
    }
    else
    {
        char letter = en_passant[0];
        int file = letter - 96;
        int rank = en_passant[1] - 48;
        enPassantSquare = Square((rank - 1) * 8 + file - 1);
    }

    // half_move_clock
    halfMoveClock = std::stoi(half_move_clock);

    // full_move_counter actually half moves
    fullMoveNumber = std::stoi(full_move_counter) * 2;
}

void Board::make_move(Move move)
{
    Piece p = piece_at(move.from_sq);
    PieceType pt = type_of_piece(p);

    Square from_sq = move.from_sq;
    Square to_sq = move.to_sq;
    Piece capture = piece_at(move.to_sq);

    assert(from_sq >= 0 && from_sq < 64);
    assert(to_sq >= 0 && to_sq < 64);
    assert(type_of_piece(capture) != KING);
    assert(p != NONE);

    // *****************************
    // STORE STATE HISTORY
    // *****************************

    prev_boards.emplace_back(enPassantSquare, castlingRights, halfMoveClock, capture);

    halfMoveClock++;
    fullMoveNumber++;

    const bool ep = to_sq == enPassantSquare;

    // Castling is encoded as king captures rook
    const bool isCastling = pt == KING && std::abs(from_sq - to_sq) == 2;

    // *****************************
    // UPDATE HASH
    // *****************************

    enPassantSquare = NO_SQ;

    if (pt == KING)
    {
        removeCastlingRightsAll(sideToMove);

        if (isCastling)
        {
            const Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
            const Square rookFromSq = file_rank_square(to_sq > from_sq ? FILE_H : FILE_A, square_rank(from_sq));
            const Square rookToSq = file_rank_square(to_sq > from_sq ? FILE_F : FILE_D, square_rank(from_sq));

            remove_piece(rook, rookFromSq);
            place_piece(rook, rookToSq);
        }
    }
    else if (pt == ROOK)
    {
        removeCastlingRightsRook(from_sq);
    }
    else if (pt == PAWN)
    {
        halfMoveClock = 0;
        if (ep)
        {
            remove_piece(make_piece(PAWN, ~sideToMove), Square(to_sq ^ 8));
        }
        else if (std::abs(from_sq - to_sq) == 16)
        {
            Bitboard epMask = PawnAttacks(Square(to_sq ^ 8), sideToMove);
            if (epMask & pieces(PAWN, ~sideToMove))
            {
                enPassantSquare = Square(to_sq ^ 8);

                assert(piece_at(enPassantSquare) == NONE);
            }
        }
    }

    if (capture != NONE)
    {
        halfMoveClock = 0;
        if (type_of_piece(capture) == ROOK)
            removeCastlingRightsRook(to_sq);

        remove_piece(capture, to_sq);
    }

    if (move.promotion_piece != NONETYPE)
    {
        halfMoveClock = 0;
        remove_piece(make_piece(PAWN, sideToMove), from_sq);
        place_piece(make_piece(move.promotion_piece, sideToMove), to_sq);
    }
    else
    {
        assert(piece_at(to_sq) == NONE);

        remove_piece(p, from_sq);
        place_piece(p, to_sq);
    }

    sideToMove = ~sideToMove;
}

void Board::unmake_move(Move move)
{
    const auto restore = prev_boards.back();
    prev_boards.pop_back();

    enPassantSquare = restore.enPassant;
    castlingRights = restore.castling;
    halfMoveClock = restore.halfMove;
    Piece capture = restore.capturedPiece;

    fullMoveNumber--;

    Square from_sq = move.from_sq;
    Square to_sq = move.to_sq;
    bool promotion = move.promotion_piece != NONETYPE;

    sideToMove = ~sideToMove;
    PieceType pt = type_of_piece(piece_at(move.to_sq));
    Piece p = make_piece(pt, sideToMove);

    const bool isCastling = pt == KING && std::abs(from_sq - to_sq) == 2;

    if (isCastling)
    {
        Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
        const Square rookFromSq = file_rank_square(to_sq > from_sq ? FILE_H : FILE_A, square_rank(from_sq));
        const Square rookToSq = file_rank_square(to_sq > from_sq ? FILE_F : FILE_D, square_rank(from_sq));

        // We need to remove both pieces first and then place them back.
        remove_piece(p, to_sq);
        remove_piece(rook, rookToSq);

        place_piece(rook, rookFromSq);
        place_piece(p, from_sq);

        return;
    }
    else if (promotion)
    {
        remove_piece(p, to_sq);
        place_piece(make_piece(PAWN, sideToMove), from_sq);
        if (capture != NONE)
            place_piece(capture, to_sq);

        return;
    }
    else
    {
        remove_piece(p, to_sq);
        place_piece(p, from_sq);
    }

    if (to_sq == enPassantSquare && pt == PAWN)
    {
        place_piece(make_piece(PAWN, ~sideToMove), Square(enPassantSquare ^ 8));
    }
    else if (capture != NONE)
    {
        place_piece(capture, to_sq);
    }
}

Bitboard Board::us(Color c)
{
    return pieceBB[c][PAWN] | pieceBB[c][KNIGHT] | pieceBB[c][BISHOP] | pieceBB[c][ROOK] | pieceBB[c][QUEEN] |
           pieceBB[c][KING];
}

Bitboard Board::allBB()
{
    return us(WHITE) | us(BLACK);
}

void Board::removeCastlingRightsAll(Color c)
{
    if (c == WHITE)
    {
        castlingRights &= ~(WK | WQ);
    }
    else
    {
        castlingRights &= ~(BK | BQ);
    }
}

void Board::removeCastlingRightsRook(Square sq)
{

    if (castlingMapRook.find(sq) != castlingMapRook.end())
    {
        castlingRights &= ~castlingMapRook[sq];
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
                SQUARES_BETWEEN_BB[sq1][sq2] = 0ull;
            else if (square_file(sq1) == square_file(sq2) || square_rank(sq1) == square_rank(sq2))
                SQUARES_BETWEEN_BB[sq1][sq2] = RookAttacks(sq1, sqs) & RookAttacks(sq2, sqs);
            else if (diagonal_of(sq1) == diagonal_of(sq2) || anti_diagonal_of(sq1) == anti_diagonal_of(sq2))
                SQUARES_BETWEEN_BB[sq1][sq2] = BishopAttacks(sq1, sqs) & BishopAttacks(sq2, sqs);
        }
    }
}

Square Board::KingSQ(Color c) const
{
    return lsb(pieces(KING, c));
}

Board::Board()
{
    initializeLookupTables();
}

Board::~Board()
{
}

std::ostream &operator<<(std::ostream &os, const Board &b)
{
    for (int i = 63; i >= 0; i -= 8)
    {
        os << " " << pieceToChar[b.board[i - 7]] << " " << pieceToChar[b.board[i - 6]] << " "
           << pieceToChar[b.board[i - 5]] << " " << pieceToChar[b.board[i - 4]] << " " << pieceToChar[b.board[i - 3]]
           << " " << pieceToChar[b.board[i - 2]] << " " << pieceToChar[b.board[i - 1]] << " " << pieceToChar[b.board[i]]
           << " \n";
    }
    os << "\n\n";
    // os << "Fen: " << b.getFen() << "\n";
    os << "Side to move: " << static_cast<int>(b.sideToMove) << "\n";
    os << "Castling rights: " << static_cast<int>(b.castlingRights) << "\n";
    os << "Halfmoves: " << static_cast<int>(b.halfMoveClock) << "\n";
    os << "Fullmoves: " << static_cast<int>(b.fullMoveNumber) / 2 << "\n";
    os << "EP: " << static_cast<int>(b.enPassantSquare) << "\n";

    os << std::endl;
    return os;
}

std::string uciMove(Move move)
{
    std::stringstream ss;

    // Add the from and to squares to the string stream
    ss << squareToString[move.from_sq];
    ss << squareToString[move.to_sq];

    // If the move is a promotion, add the promoted piece to the string stream
    if (move.promotion_piece != NONETYPE)
    {
        ss << PieceTypeToPromPiece[move.promotion_piece];
    }

    return ss.str();
}
