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

    // Rotates the book vector based on the starting offset
    void rotate(std::size_t offset) {
        const auto rotate = [offset](auto& vec) {
            std::rotate(vec.begin(), vec.begin() + (offset % vec.size()), vec.end());
        };

        std::visit(rotate, book_);
    }

    // Gets rid of unused openings within the book vector to reduce memory usage
    void truncate(std::size_t rounds) {
        const auto truncate = [rounds](auto& vec) {
            if (vec.size() > rounds) {
                vec.resize(rounds);
            }
        };

        std::visit(truncate, book_);
    }

    // Shrink book vector
    void shrink() {
        const auto shrink = [](auto& vec) { vec.shrink_to_fit(); };

        std::visit(shrink, book_);
    }

    [[nodiscard]] std::optional<std::size_t> fetchId() noexcept;

    pgn::Opening operator[](std::optional<std::size_t> idx) const noexcept {
        if (!idx.has_value()) return {chess::constants::STARTPOS, {}};

        assert(idx.has_value() && idx.value() < std::get<epd_book>(book_).size());

        if (std::holds_alternative<epd_book>(book_)) {
            const auto fen = std::get<epd_book>(book_)[*idx];
            return {std::string(fen), {}, chess::Board(fen).sideToMove()};
        }

        return std::get<pgn_book>(book_)[*idx];
    }

   private:
    void setup(const std::string& file, FormatType type);

    using epd_book = std::vector<std::string>;
    using pgn_book = std::vector<pgn::Opening>;

    std::size_t start_         = 0;
    std::size_t opening_index_ = 0;
    std::size_t offset_        = 0;
    int rounds_;
    int games_;
    int plies_;
    OrderType order_;
    std::variant<epd_book, pgn_book> book_;
};

}  // namespace fast_chess::book
