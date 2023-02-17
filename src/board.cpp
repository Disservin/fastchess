#include "board.h"

#include <vector>

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

void Board::load_fen(const std::string &fen)
{
    for (auto c : {WHITE, BLACK})
    {
        for (PieceType p = PAWN; p < NONETYPE; p++)
        {
            pieceBB[c][p] = 0ULL;
        }
    }

    std::vector<std::string> params = splitInput(fen);

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

void Board::make_move(Move move)
{
}

void Board::unmake_move(Move move)
{
}