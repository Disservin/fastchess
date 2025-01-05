#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <core/helper.hpp>
#include <game/book/opening.hpp>

#include <chess.hpp>

namespace fastchess::book {

class PgnReader {
   public:
    PgnReader() = default;
    explicit PgnReader(const std::string& pgn_file_path, int plies_limit = -1);

    // Extracts all pgns from the file
    [[nodiscard]] std::vector<Opening>& get() noexcept { return pgns_; }
    [[nodiscard]] const std::vector<Opening>& get() const noexcept { return pgns_; }

   private:
    std::vector<Opening> pgns_;
    std::string file_name_;
    int plies_limit_;
};

}  // namespace fastchess::book
