#include "perft.hpp"

#include <iostream>

#include "chess/board.hpp"
#include "chess/movegen.hpp"

namespace fast_chess {
uint64_t Perft::perftFunction(Board &b, int depth, int max) {
    Movelist moves;
    Movegen::legalmoves(b, moves);
    if (depth == 0)
        return 1;
    else if (depth == 1 && max != 1) {
        return moves.size;
    }
    uint64_t nodes_c = 0;

    for (const auto move : moves) {
        b.makeMove(move);
        nodes_c += perftFunction(b, depth - 1, depth);
        b.unmakeMove(move);
        if (depth == max) {
            nodes_ += nodes_c;
            if (print_) std::cout << uciMove(move) << " " << nodes_c << std::endl;
            nodes_c = 0;
        }
    }
    if (depth == max && print_) std::cout << "\n" << std::endl;
    return nodes_c;
}

uint64_t Perft::getAndResetNodes() {
    auto tmp = nodes_;
    nodes_ = 0;
    return tmp;
}
}  // namespace fast_chess
