#pragma once

#include <string>
#include <variant>
#include <vector>

#include <pgn/pgn_reader.hpp>
#include <types/enums.hpp>
#include <types/tournament_options.hpp>
#include <util/rand.hpp>

namespace fast_chess {

class OpeningBook {
   public:
    OpeningBook() = default;
    explicit OpeningBook(const options::Tournament& tournament);

    /// @brief Fisher-Yates / Knuth shuffle
    void shuffle() {
        const auto shuffle = [](auto& vec) {
            for (std::size_t i = 0; i + 2 <= vec.size(); i++) {
                auto rand     = random::mersenne_rand();
                std::size_t j = i + (rand % (vec.size() - i));
                std::swap(vec[i], vec[j]);
            }
        };

        std::visit(shuffle, book_);
    }

    [[nodiscard]] Opening fetch() noexcept;

    void setInternalOffset(std::size_t offset) noexcept { matchcount_ = offset; }

   private:
    void setup(const std::string& file, FormatType type);

    using epd_book = std::vector<std::string>;
    using pgn_book = std::vector<Opening>;

    std::size_t start_ = 0;
    std::size_t matchcount_ = 0;
    int games_;
    options::Opening opening_;
    std::variant<epd_book, pgn_book> book_;
};

}  // namespace fast_chess
