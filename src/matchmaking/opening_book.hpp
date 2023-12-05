#pragma once

#include <string>
#include <variant>
#include <vector>

#include <pgn/pgn_reader.hpp>
#include <types/enums.hpp>
#include <util/rand.hpp>

namespace fast_chess {

class OpeningBook {
   public:
    OpeningBook(const std::string& file, FormatType type, std::size_t start = 0);

    /// @brief Fisher-Yates / Knuth shuffle
    void shuffle() noexcept {
        std::visit(
            [](auto& vec) {
                for (std::size_t i = 0; i + 2 <= vec.size(); i++) {
                    std::size_t j = i + (random::mersenne_rand() % (vec.size() - i));
                    std::swap(vec[i], vec[j]);
                }
            },
            book_);
    }

    [[nodiscard]] Opening fetch() noexcept;

   private:
    void setup(const std::string& file, FormatType type);

    using epd_book = std::vector<std::string>;
    using pgn_book = std::vector<Opening>;

    std::size_t start_ = 0;
    std::variant<epd_book, pgn_book> book_;
};

}  // namespace fast_chess
