#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <third_party/chess.hpp>

namespace fast_chess {

struct Opening {
    std::string fen                = chess::STARTPOS;
    std::vector<chess::Move> moves = {};
};

class PgnReader {
   public:
    explicit PgnReader(const std::string &pgn_file_path);

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
