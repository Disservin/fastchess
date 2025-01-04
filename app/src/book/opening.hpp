#pragma once

#include <string>
#include <vector>

#include <chess.hpp>

namespace fastchess::book {
struct Opening {
    Opening() = default;
    Opening(const std::string& fen_epd, const std::vector<chess::Move>& moves, chess::Color stm = chess::Color::WHITE)
        : fen_epd(fen_epd), moves(moves), stm(stm) {}

    std::string fen_epd            = chess::constants::STARTPOS;
    std::vector<chess::Move> moves = {};
    chess::Color stm               = chess::Color::WHITE;
};
}  // namespace fastchess::book
