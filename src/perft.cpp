#include <iostream>

#include "board.h"
#include "movegen.h"
#include "perft.h"

uint64_t Perft::perftFunction(Board &b, int depth, int max)
{
    Movelist moves;
    Movegen::legalmoves(b, moves);
    if (depth == 0)
        return 1;
    else if (depth == 1 && max != 1)
    {
        return moves.size;
    }
    uint64_t nodesIt = 0;

    for (auto move : moves)
    {
        b.make_move(move);
        nodesIt += perftFunction(b, depth - 1, depth);
        b.unmake_move(move);
        if (depth == max)
        {
            nodes += nodesIt;
            if (print)
                std::cout << uciMove(move) << " " << nodesIt << std::endl;
            nodesIt = 0;
        }
    }
    if (depth == max && print)
        std::cout << "\n" << std::endl;
    return nodesIt;
}

uint64_t Perft::getAndResetNodes()
{
    auto tmp = nodes;
    nodes = 0;
    return tmp;
}