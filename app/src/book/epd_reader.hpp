#pragma once

#include <string>
#include <vector>

#include <book/opening_book.hpp>

namespace fastchess::book {
class EpdReader {
   public:
    explicit EpdReader(const std::string& epd_file_path);

    [[nodiscard]] Openings getOpenings();

   private:
    const std::string epd_file_;
};
}  // namespace fastchess::book