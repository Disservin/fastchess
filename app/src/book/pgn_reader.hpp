#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <book/opening_book.hpp>

#include <chess.hpp>

namespace fastchess::book {

class PgnReader {
   public:
    explicit PgnReader(const std::string& pgn_file_path, int plies_limit = -1);

    // Extracts all pgns from the file
    [[nodiscard]] Openings getOpenings();

   private:
    std::string file_name_;
    int plies_limit_;
};

}  // namespace fastchess::book
