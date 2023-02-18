#include <vector>

#include "attacks.h"
#include "board.h"

void Board::remove_piece(Piece piece, Square sq)
{
    pieceBB[colorOf(piece)][piece] &= ~(1ULL << sq);
    board[sq] = NONE;
}

void Board::place_piece(Piece piece, Square sq)
{
    pieceBB[colorOf(piece)][piece] |= (1ULL << sq);
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

    board.reset();

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

    int indexWhite = 0;
    int indexBlack = 0;
    for (size_t i = 0; i < castling.size(); i++)
    {
        if (isupper(castling[i]))
        {
            castlingRights |= 1ull << indexWhite;
        }
        else
        {
            castlingRights |= 1ull << (2 + indexBlack);
        }
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

bool Board::make_move(Move move)
{
    auto copyPieceBB = pieceBB;
    auto copyBoard = board;
    auto copyHMC = halfMoveClock;

    Piece move_piece = piece_at(move.from_sq);
    PieceType move_pt = type_of_piece(move_piece);
    Piece capture_piece = piece_at(move.to_sq);

    prev_boards.emplace_back(enPassantSquare, castlingRights, halfMoveClock, capture_piece);

    remove_piece(move_piece, move.from_sq);

    if (capture_piece != NONE)
    {
        remove_piece(capture_piece, move.to_sq);
        halfMoveClock = 0;
    }

    if (move_pt == PAWN)
    {
        halfMoveClock = 0;

        if (move.to_sq == enPassantSquare)
        {
            remove_piece(make_piece(PAWN, ~sideToMove), Square(move.to_sq ^ 8));
        }
        if (move.promotion_piece != NONETYPE)
        {
            // set the moving piece to promotion piece
            move_piece = make_piece(move.promotion_piece, sideToMove);
        }
    }

    if (move_pt == KING && std::abs(move.to_sq - move.from_sq) == 2)
    {
        const Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
        Square rookFromSq = file_rank_square(move.to_sq > move.from_sq ? FILE_H : FILE_A, square_rank(move.from_sq));
        Square rookToSq = file_rank_square(move.to_sq > move.from_sq ? FILE_F : FILE_D, square_rank(move.from_sq));

        remove_piece(rook, rookFromSq);
        place_piece(rook, rookToSq);
    }

    place_piece(move_piece, move.to_sq);

    if (isKingAttacked(~sideToMove, lsb(pieces<KING>(sideToMove))))
    {
        pieceBB = copyPieceBB;
        board = copyBoard;
        halfMoveClock = copyHMC;

        return false;
    }

    fullMoveNumber++;
    sideToMove = ~sideToMove;

    return true;
}

void Board::unmake_move(Move move)
{
    const auto restore = prev_boards.back();
    prev_boards.pop_back();

    Piece move_piece = piece_at(move.to_sq);
    PieceType move_pt = type_of_piece(move_piece);
    Piece capture_piece = restore.capturedPiece;

    enPassantSquare = restore.enPassant;
    castlingRights = restore.castling;
    halfMoveClock = restore.halfMove;
    fullMoveNumber--;
    sideToMove = ~sideToMove;

    place_piece(move_piece, move.from_sq);
    remove_piece(move_piece, move.to_sq);

    if (capture_piece != NONE)
        place_piece(capture_piece, move.to_sq);

    if (move_pt == PAWN)
    {
        if (move.to_sq == enPassantSquare)
        {
            place_piece(make_piece(PAWN, ~sideToMove), Square(move.to_sq ^ 8));
        }
        if (move.promotion_piece != NONETYPE)
        {
            // set the moving piece to promotion piece
            remove_piece(move_piece, move.from_sq);
            place_piece(make_piece(PAWN, sideToMove), move.from_sq);
        }
    }

    if (move_pt == KING && std::abs(move.to_sq - move.from_sq) == 2)
    {
        const Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
        Square rookFromSq = file_rank_square(move.to_sq > move.from_sq ? FILE_H : FILE_A, square_rank(move.from_sq));
        Square rookToSq = file_rank_square(move.to_sq > move.from_sq ? FILE_F : FILE_D, square_rank(move.from_sq));

        place_piece(rook, rookFromSq);
        remove_piece(rook, rookToSq);
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

Board::Board()
{
}

Board::~Board()
{
}