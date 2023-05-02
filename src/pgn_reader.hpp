#pragma once

#include <string>
#include <vector>
#include <fstream>

#include <third_party/chess.hpp>

namespace fast_chess {

struct Opening {
    std::string fen = Chess::STARTPOS;
    std::vector<std::string> moves;
};
class PgnReader {
   public:
    explicit PgnReader(const std::string &pgn_file_path);

    std::vector<Opening> getPgns() const;

    static std::string extractHeader(const std::string &line);

    static std::vector<std::string> extractMoves(std::string line);

   private:
    void analyseFile();

    std::vector<Opening> pgns_;
    std::ifstream pgn_file_;
};

}  // namespace fast_chess
