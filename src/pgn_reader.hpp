#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <third_party/chess.hpp>

namespace fast_chess {

struct Opening {
    std::string fen = chess::STARTPOS;
    std::vector<std::string> moves = {};
};

class PgnReader {
   public:
    explicit PgnReader(const std::string &pgn_file_path);

    /// @brief
    /// @return
    [[nodiscard]] std::vector<Opening> getPgns() const;

    /// @brief Extracts the data from a header line. i.e. [Result "1-0"] -> "1-0"
    /// @param line
    /// @return
    [[nodiscard]] static std::string extractHeader(const std::string &line);

    /// @brief Extracts all moves from the pgn game. Only Pgn Books with SAN Notation are supported.
    /// @param line
    /// @return
    [[nodiscard]] static std::vector<std::string> extractMoves(std::string line);

   private:
    /// @brief Extracts all pgns from the file and stores them in pgns_
    void analyseFile();

    std::vector<Opening> pgns_;
    std::ifstream pgn_file_;
};

}  // namespace fast_chess
