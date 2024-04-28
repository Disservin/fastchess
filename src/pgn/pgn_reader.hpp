#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <chess.hpp>

namespace fast_chess::pgn {

struct Opening {
    Opening() = default;
    Opening(const std::string& fen, const std::vector<chess::Move>& moves,
            chess::Color stm = chess::Color::WHITE)
        : fen(fen), moves(moves), stm(stm) {}

    std::string fen                = chess::constants::STARTPOS;
    std::vector<chess::Move> moves = {};
    chess::Color stm               = chess::Color::WHITE;
};

class PgnReader {
   public:
    explicit PgnReader(const std::string& pgn_file_path, int plies_limit = -1);

    [[nodiscard]] std::vector<Opening> getOpenings();

   private:
    // Extracts all pgns from the file
    std::vector<Opening> analyseFile();

    std::ifstream pgn_file_;
    int plies_limit_;
};

}  // namespace fast_chess::pgn
