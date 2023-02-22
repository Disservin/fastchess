#include <bitset>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "attacks.h"
#include "board.h"
#include "zobrist.h"

void Board::loadFen(const std::string &fen)
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
        char letter = en_passant[0];
        int file = letter - 96;
        int rank = en_passant[1] - 48;
        enPassantSquare = Square((rank - 1) * 8 + file - 1);
    }

    // half_move_clock
    halfMoveClock = std::stoi(half_move_clock);

    // full_move_counter actually half moves
    fullMoveNumber = std::stoi(full_move_counter) * 2;

    hashKey = zobristHash();
}

void Board::makeMove(Move move)
{
    Square from_sq = move.from_sq;
    Square to_sq = move.to_sq;
    Piece p = pieceAt(from_sq);
    PieceType pt = typeOfPiece(p);
    Piece capture = pieceAt(move.to_sq);

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
    const bool isCastling = pt == KING && std::abs(from_sq - to_sq) == 2;

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
            const Square rookFromSq = fileRankSquare(to_sq > from_sq ? FILE_H : FILE_A, squareRank(from_sq));
            const Square rookToSq = fileRankSquare(to_sq > from_sq ? FILE_F : FILE_D, squareRank(from_sq));

            removePiece(rook, rookFromSq);
            placePiece(rook, rookToSq);
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

    Square from_sq = move.from_sq;
    Square to_sq = move.to_sq;
    Piece capture = restore.capturedPiece;
    PieceType pt = typeOfPiece(pieceAt(to_sq));

    sideToMove = ~sideToMove;

    Piece p = make_piece(pt, sideToMove);

    bool promotion = move.promotion_piece != NONETYPE;

    if (pt == KING && std::abs(from_sq - to_sq) == 2)
    {
        Piece rook = sideToMove == WHITE ? WHITEROOK : BLACKROOK;
        const Square rookFromSq = fileRankSquare(to_sq > from_sq ? FILE_H : FILE_A, squareRank(from_sq));
        const Square rookToSq = fileRankSquare(to_sq > from_sq ? FILE_F : FILE_D, squareRank(from_sq));

        // We need to remove both pieces first and then place them back.
        removePiece(p, to_sq);
        removePiece(rook, rookToSq);

        placePiece(rook, rookFromSq);
        placePiece(p, from_sq);

        return;
    }
    else if (promotion)
    {
        removePiece(p, to_sq);
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
        Square sq = poplsb(wPieces);
        hash ^= updateKeyPiece(pieceAt(sq), sq);
    }
    while (bPieces)
    {
        Square sq = poplsb(bPieces);
        hash ^= updateKeyPiece(pieceAt(sq), sq);
    }
    // Ep hash
    uint64_t ep_hash = 0ULL;
    if (enPassantSquare != NO_SQ)
    {
        ep_hash = updateKeyEnPassant(enPassantSquare);
    }
    // Turn hash
    uint64_t turn_hash = sideToMove == WHITE ? RANDOM_ARRAY[780] : 0;
    // Castle hash
    uint64_t cast_hash = updateKeyCastling();

    return hash ^ cast_hash ^ turn_hash ^ ep_hash;
}

bool Board::isRepetition(int draw) const
{
    uint8_t c = 0;

    for (int i = static_cast<int>(hashHistory.size()) - 2;
         i >= 0 && i >= static_cast<int>(hashHistory.size()) - halfMoveClock - 1; i -= 2)
    {
        if (hashHistory[i] == hashKey)
            c++;
        if (c == draw)
            return true;
    }

    return false;
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

bool Board::isKingAttacked(Color c, Square sq) const
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
    char letter = squareStr[0];
    int file = letter - 96;
    int rank = squareStr[1] - 48;
    int index = (rank - 1) * 8 + file - 1;
    return Square(index);
}

Move convertUciToMove(const std::string &input)
{
    Square source = extractSquare(input.substr(0, 2));
    Square target = extractSquare(input.substr(2, 2));
    switch (input.length())
    {
    case 4:
        return Move(source, target, NONETYPE);
    case 5:
        return Move(source, target, charToPieceType[input.at(4)]);
    default:
        throw std::runtime_error("Cant parse move" + input);
        return Move(NO_SQ, NO_SQ, NONETYPE);
    }
}