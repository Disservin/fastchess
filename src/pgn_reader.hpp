#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <chess.hpp>

namespace fast_chess {

struct Opening {
    Opening() = default;
    Opening(const std::string& fen, const std::vector<chess::Move>& moves)
        : fen(fen), moves(moves) {}

    std::string fen                = chess::constants::STARTPOS;
    std::vector<chess::Move> moves = {};
};

class PgnReader {
   public:
    explicit PgnReader(const std::string& pgn_file_path);

    /// @brief
    /// @return
    [[nodiscard]] std::vector<Opening> getPgns() const;

   private:
    /// @brief Extracts all pgns from the file and stores them in pgns_
    void analyseFile();

    std::ifstream pgn_file_;
    std::vector<Opening> pgns_;
};

}  // namespace fast_chess
