#pragma once

#include <string>
#include <vector>

#include <core/helper.hpp>
#include <core/memory/heap_str.hpp>

namespace fastchess::book {
class EpdReader {
   public:
    EpdReader() = default;
    explicit EpdReader(const std::string& epd_file_path);

    [[nodiscard]] auto& get() noexcept { return openings_; }
    [[nodiscard]] const auto& get() const noexcept { return openings_; }

   private:
    std::vector<util::heap_string> openings_;
    std::string epd_file_;
};
}  // namespace fastchess::book