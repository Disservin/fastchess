#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <core/config/config.hpp>
#include <core/helper.hpp>
#include <core/rand.hpp>
#include <game/book/epd_reader.hpp>
#include <game/book/pgn_reader.hpp>
#include <types/enums.hpp>
#include <types/tournament.hpp>

namespace fastchess::book {

struct Openings {
    // Fisher-Yates / Knuth shuffle
    template <typename T>
    static void shuffle(T& vec) {
        for (std::size_t i = 0; i + 2 <= vec.size(); i++) {
            auto rand     = random::mersenne_rand();
            std::size_t j = i + (rand % (vec.size() - i));
            std::swap(vec[i], vec[j]);
        }
    }

    // Rotates the book vector based on the starting offset
    template <typename T>
    static void rotate(T& vec, std::size_t offset) {
        std::rotate(vec.begin(), vec.begin() + (offset % vec.size()), vec.end());
    }

    // Gets rid of unused openings within the book vector to reduce memory usage
    template <typename T>
    static void truncate(T& vec, std::size_t rounds) {
        if (vec.size() > rounds) {
            vec.resize(rounds);
        }
    }

    // Shrink book vector
    template <typename T>
    static void shrink(T& vec) {
        using BookType = std::decay_t<decltype(vec)>;
        std::vector<typename BookType::value_type> tmp(vec.begin(), vec.end());
        vec.swap(tmp);
        vec.shrink_to_fit();
    }
};

class OpeningBook {
   public:
    OpeningBook() = default;
    explicit OpeningBook(const config::Tournament& config, std::size_t initial_matchcount = 0);

    [[nodiscard]] std::optional<std::size_t> fetchId() noexcept;

    [[nodiscard]] Opening operator[](std::optional<std::size_t> idx) const noexcept;

   private:
    void setup(const std::string& file, FormatType type);

    std::size_t start_         = 0;
    std::size_t opening_index_ = 0;
    std::size_t offset_        = 0;
    std::size_t rounds_;
    std::size_t games_;
    int plies_;
    OrderType order_;

    std::variant<EpdReader, PgnReader> openings_;
};

}  // namespace fastchess::book
