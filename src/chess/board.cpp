#include <algorithm>
#include <bitset>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../options.hpp"
#include "attacks.hpp"
#include "board.hpp"
#include "movegen.hpp"
#include "zobrist.hpp"

void Board::loadFen(const std::string &fen)
{
    for (const auto c : {WHITE, BLACK})
    {
        for (PieceType p = PAWN; p < NONETYPE; p++)
        {
            pieceBB[c][p] = 0ULL;
        }
    }

    const std::vector<std::string> params = CMD::Options::splitString(fen, ' ');

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
        const char letter = en_passant[0];
        const int file = letter - 96;
        const int rank = en_passant[1] - 48;
        enPassantSquare = Square((rank - 1) * 8 + file - 1);
    }

    // half_move_clock
    halfMoveClock = std::stoi(half_move_clock);

    // full_move_counter actually half moves
    fullMoveNumber = std::stoi(full_move_counter) * 2;

    hashKey = zobristHash();
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
    ss << (sideToMove == WHITE ? " w " : " b ");

    // Append the appropriate characters to the FEN string to indicate
    // whether or not castling is allowed for each player
    if (castlingRights & WK)
        ss << "K";
    if (castlingRights & WQ)
        ss << "Q";
    if (castlingRights & BK)
        ss << "k";
    if (castlingRights & BQ)
        ss << "q";
    if (castlingRights == 0)
        ss << "-";

    // Append information about the en passant square (if any)
    // and the halfmove clock and fullmove number to the FEN string
    if (enPassantSquare == NO_SQ)
        ss << " - ";
    else
        ss << " " << squareToString[enPassantSquare] << " ";

    ss << int(halfMoveClock) << " " << int(fullMoveNumber / 2);

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

    prevBoards.emplace_back(enPassantSquare, castlingRights, halfMoveClock, capture);
    hashHistory.emplace_back(hashKey);

    halfMoveClock++;
    fullMoveNumber++;

    const bool ep = to_sq == enPassantSquare;
    const bool isCastling = pt == KING && typeOfPiece(capture) == ROOK && colorOf(from_sq) == colorOf(to_sq);

    if (enPassantSquare != NO_SQ)
        hashKey ^= updateKeyEnPassant(enPassantSquare);

    hashKey ^= updateKeyCastling();
    enPassantSquare = NO_SQ;

    if (pt == KING)
    {
        removeCastlingRightsAll(sideToMove);

        if (isCastling)
        {
            const Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
            Square rookToSq = fileRankSquare(to_sq > from_sq ? FILE_F : FILE_D, squareRank(from_sq));
            Square kingToSq = fileRankSquare(to_sq > from_sq ? FILE_G : FILE_C, squareRank(from_sq));
            removePiece(p, from_sq);
            removePiece(rook, to_sq);

            placePiece(p, kingToSq);
            placePiece(rook, rookToSq);

            sideToMove = ~sideToMove;

            hashKey ^= updateKeySideToMove();
            hashKey ^= updateKeyCastling();

            return true;
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
            removePiece(make_piece(PAWN, ~sideToMove), Square(to_sq ^ 8));
        }
        else if (std::abs(from_sq - to_sq) == 16)
        {
            Bitboard epMask = PawnAttacks(Square(to_sq ^ 8), sideToMove);
            if (epMask & pieces(PAWN, ~sideToMove))
            {
                enPassantSquare = Square(to_sq ^ 8);
                hashKey ^= updateKeyEnPassant(enPassantSquare);

                assert(pieceAt(enPassantSquare) == NONE);
            }
        }
    }

    if (capture != NONE)
    {
        halfMoveClock = 0;
        if (typeOfPiece(capture) == ROOK)
            removeCastlingRightsRook(to_sq);

        removePiece(capture, to_sq);
    }

    if (move.promotion_piece != NONETYPE)
    {
        halfMoveClock = 0;
        removePiece(make_piece(PAWN, sideToMove), from_sq);
        placePiece(make_piece(move.promotion_piece, sideToMove), to_sq);
    }
    else
    {
        assert(pieceAt(to_sq) == NONE);

        removePiece(p, from_sq);
        placePiece(p, to_sq);
    }

    hashKey ^= updateKeySideToMove();
    hashKey ^= updateKeyCastling();

    sideToMove = ~sideToMove;

    return true;
}

void Board::unmakeMove(Move move)
{
    const auto restore = prevBoards.back();
    prevBoards.pop_back();

    hashKey = hashHistory.back();
    hashHistory.pop_back();

    enPassantSquare = restore.enPassant;
    castlingRights = restore.castling;
    halfMoveClock = restore.halfMove;

    fullMoveNumber--;

    const Square from_sq = move.from_sq;
    const Square to_sq = move.to_sq;
    const Piece capture = restore.capturedPiece;
    const PieceType pt = move.moving_piece;

    sideToMove = ~sideToMove;

    const Piece p = make_piece(pt, sideToMove);

    const bool promotion = move.promotion_piece != NONETYPE;

    if ((p == WHITEKING && capture == WHITEROOK) || (p == BLACKKING && capture == BLACKROOK))
    {
        Square rookToSq = to_sq;
        Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
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
        placePiece(make_piece(PAWN, sideToMove), from_sq);
        if (capture != NONE)
            placePiece(capture, to_sq);

        return;
    }
    else
    {
        removePiece(p, to_sq);
        placePiece(p, from_sq);
    }

    if (to_sq == enPassantSquare && pt == PAWN)
    {
        placePiece(make_piece(PAWN, ~sideToMove), Square(enPassantSquare ^ 8));
    }
    else if (capture != NONE)
    {
        placePiece(capture, to_sq);
    }
}

Bitboard Board::us(Color c) const
{
    return pieceBB[c][PAWN] | pieceBB[c][KNIGHT] | pieceBB[c][BISHOP] | pieceBB[c][ROOK] | pieceBB[c][QUEEN] |
           pieceBB[c][KING];
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
    return hashKey;
}

Bitboard Board::pieces(PieceType type, Color color) const
{
    return pieceBB[color][type];
}

Piece Board::pieceAt(Square square) const
{
    return board[square];
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
    if (enPassantSquare != NO_SQ)
    {
        ep_hash = updateKeyEnPassant(enPassantSquare);
    }
    // Turn hash
    const uint64_t turn_hash = sideToMove == WHITE ? RANDOM_ARRAY[780] : 0;
    // Castle hash
    const uint64_t cast_hash = updateKeyCastling();

    return hash ^ cast_hash ^ turn_hash ^ ep_hash;
}

bool Board::isRepetition() const
{
    uint8_t c = 0;

    for (int i = static_cast<int>(hashHistory.size()) - 2;
         i >= 0 && i >= static_cast<int>(hashHistory.size()) - halfMoveClock - 1; i -= 2)
    {
        if (hashHistory[i] == hashKey)
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

    if (halfMoveClock >= 100)
    {
        if (isSquareAttacked(~sideToMove, lsb(pieces(KING, sideToMove))) && !Movegen::hasLegalMoves(*this))
            return GameResult(~sideToMove);
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
        if (isSquareAttacked(~sideToMove, lsb(pieces(KING, sideToMove))))
            return GameResult(~sideToMove);
        return GameResult::DRAW;
    }

    return GameResult::NONE;
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
            else if (squareFile(sq1) == squareFile(sq2) || squareRank(sq1) == squareRank(sq2))
                SQUARES_BETWEEN_BB[sq1][sq2] = RookAttacks(sq1, sqs) & RookAttacks(sq2, sqs);
            else if (diagonalOf(sq1) == diagonalOf(sq2) || anti_diagonalOf(sq1) == anti_diagonalOf(sq2))
                SQUARES_BETWEEN_BB[sq1][sq2] = BishopAttacks(sq1, sqs) & BishopAttacks(sq2, sqs);
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
    return std::max(std::abs(squareFile(a) - squareFile(b)), std::abs(squareRank(a) - squareRank(b)));
}

void Board::placePiece(Piece piece, Square sq)
{
    hashKey ^= updateKeyPiece(piece, sq);
    pieceBB[colorOf(piece)][typeOfPiece(piece)] |= (1ULL << sq);
    board[sq] = piece;
}

void Board::removePiece(Piece piece, Square sq)
{
    hashKey ^= updateKeyPiece(piece, sq);
    pieceBB[colorOf(piece)][typeOfPiece(piece)] &= ~(1ULL << sq);
    board[sq] = NONE;
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
    return castlingKey[castlingRights];
}

uint64_t Board::updateKeySideToMove() const
{
    return RANDOM_ARRAY[780];
}

Board::Board()
{
    initializeLookupTables();
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
    os << "Side to move: " << static_cast<int>(b.sideToMove) << "\n";
    os << "Castling rights: " << static_cast<int>(b.castlingRights) << "\n";
    os << "Halfmoves: " << static_cast<int>(b.halfMoveClock) << "\n";
    os << "Fullmoves: " << static_cast<int>(b.fullMoveNumber) / 2 << "\n";
    os << "EP: " << static_cast<int>(b.enPassantSquare) << "\n";

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
        move.to_sq =
            Board::fileRankSquare(move.to_sq > move.from_sq ? FILE_G : FILE_C, Board::squareRank(move.from_sq));
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

    if (Board::typeOfPiece(board.pieceAt(source)) == KING && Board::squareDistance(target, source) == 2)
    {
        target = Board::fileRankSquare(target > source ? FILE_H : FILE_A, Board::squareRank(source));
    }

    switch (input.length())
    {
    case 4:
        return Move(source, target, Board::typeOfPiece(board.pieceAt(source)), NONETYPE);
    case 5:
        return Move(source, target, Board::typeOfPiece(board.pieceAt(source)), charToPieceType[input.at(4)]);
    default:
        throw std::runtime_error("Cant parse move" + input);
        return Move(NO_SQ, NO_SQ, NONETYPE, NONETYPE);
    }
}

std::string MoveToSan(Board &b, Move move)
{
    static const std::string sanPieceType[] = {"", "N", "B", "R", "Q", "K"};
    static const std::string sanFile[] = {"a", "b", "c", "d", "e", "f", "g", "h"};

    const PieceType pt = move.moving_piece;

    assert(b.pieceAt(move.from_sq) != NONE);

    if ((pt == WHITEKING && b.pieceAt(move.to_sq) == WHITEROOK) ||
        (pt == BLACKKING && b.pieceAt(move.to_sq) == BLACKROOK))
    {
        if (Board::squareFile(move.to_sq) < Board::squareFile(move.from_sq))
            return "O-O-O";
        else
            return "O-O";
    }

    std::string san;
    if (pt != PAWN)
        san = sanPieceType[pt];

    // ambiguous move
    Movelist moves;
    Movegen::legalmoves(b, moves);

    for (const auto &cand : moves)
    {
        if (pt != PAWN && move != cand && Board::typeOfPiece(b.pieceAt(cand.from_sq)) == pt && move.to_sq == cand.to_sq)
        {
            if (Board::squareFile(move.from_sq) == Board::squareFile(cand.from_sq))
                san += std::to_string(Board::squareRank(move.from_sq) + 1);
            else
                san += sanFile[Board::squareFile(move.from_sq)];
            break;
        }
    }

    // capture
    if (b.pieceAt(move.to_sq) != NONE || (pt == PAWN && b.enPassantSquare == move.to_sq))
        san += (pt == PAWN ? sanFile[Board::squareFile(move.from_sq)] : "") + "x";

    san += sanFile[Board::squareFile(move.to_sq)];
    san += std::to_string(Board::squareRank(move.to_sq) + 1);

    if (move.promotion_piece != NONETYPE)
        san += "=" + sanPieceType[move.promotion_piece];

    b.makeMove(move);

    moves.size = 0;
    Movegen::legalmoves(b, moves);

    const bool inCheck = b.isSquareAttacked(~b.sideToMove, lsb(b.pieces<KING>(b.sideToMove)));

    b.unmakeMove(move);

    if (moves.size == 0 && inCheck)
        return san + "#";

    if (inCheck)
        return san + "+";

    return san;
}

std::string resultToString(GameResult result)
{
    if (result == GameResult::BLACK_WIN)
    {
        return "0-1";
    }
    else if (result == GameResult::WHITE_WIN)
    {
        return "1-0";
    }
    else if (result == GameResult::DRAW)
    {
        return "1/2-1/2";
    }
    else
    {
        return "*";
    }
}
