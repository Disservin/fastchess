#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <pgn/pgn_reader.hpp>
#include <types/enums.hpp>
#include <types/tournament_options.hpp>
#include <util/rand.hpp>

namespace fast_chess::book {

class OpeningBook {
   public:
    OpeningBook() = default;
    explicit OpeningBook(const options::Tournament& tournament, std::size_t initial_matchcount = 0);

    // Fisher-Yates / Knuth shuffle
    void shuffle() {
        const auto shuffle = [](auto& vec) {
            for (std::size_t i = 0; i + 2 <= vec.size(); i++) {
                auto rand     = util::random::mersenne_rand();
                std::size_t j = i + (rand % (vec.size() - i));
                std::swap(vec[i], vec[j]);
            }
        };

        std::visit(shuffle, book_);
    }

    [[nodiscard]] std::optional<std::size_t> fetchId() noexcept;

    [[nodiscard]] pgn::Opening operator[](std::optional<std::size_t> idx) {
        if (!idx.has_value()) return {chess::constants::STARTPOS, {}};

        if (std::holds_alternative<epd_book>(book_)) {
            const auto pos = std::get<epd_book>(book_)[*idx];
            const auto fen = readLineFromEpdBook(pos.first, pos.second);

            return {fen, {}, chess::Board(fen).sideToMove()};
        }

        return std::get<pgn_book>(book_)[*idx];
    }

   private:
    void setup(const std::string& file, FormatType type);

    [[nodiscard]] std::string readLineFromEpdBook(std::streampos start, std::streampos end);

    using epd_book = std::vector<std::pair<std::streampos, std::streampos>>;
    using pgn_book = std::vector<pgn::Opening>;

    std::size_t opening_index_ = 0;
    std::size_t start_         = 0;
    std::size_t matchcount_    = 0;
    int games_;
    int plies_;
    OrderType order_;
    std::variant<epd_book, pgn_book> book_;

    std::ifstream opening_file_;
};

}  // namespace fast_chess::book
