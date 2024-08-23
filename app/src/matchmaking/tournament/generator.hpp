#pragma once

#include <config/config.hpp>
#include <optional>
#include <tuple>
#include <vector>

#include <book/opening_book.hpp>

namespace fastchess {

class MatchGenerator {
   public:
    MatchGenerator() : i(0), j(1), k(0), g(0), engine_configs_size(config::EngineConfigs.get().size()) {}

    void setup(book::OpeningBook* opening_book, int initial_matchcount) {
        opening_book_ = opening_book;
        offset_       = initial_matchcount / config::TournamentConfig.get().games;
    }

    std::optional<std::tuple<std::size_t, std::size_t, std::size_t, int, std::optional<std::size_t>>> next() {
        if (i >= engine_configs_size) {
            return std::nullopt;  // No more matches to generate
        }

        if (k >= config::TournamentConfig.get().rounds) {
            advance();
            return next();  // Skip to the next valid state
        }

        if (g >= config::TournamentConfig.get().games) {
            k++;
            g = 0;
            return next();  // Skip to the next valid state
        }

        const auto opening = opening_book_->fetchId();
        auto match         = std::make_tuple(i, j, k, g, opening);

        g++;  // Move to the next game

        return match;
    }

   private:
    std::size_t i, j, k, g;
    std::size_t engine_configs_size;
    book::OpeningBook* opening_book_ = nullptr;
    int offset_;

    void advance() {
        j++;
        if (j >= engine_configs_size) {
            i++;
            j = i + 1;
        }
        k = offset_;
    }
};

}  // namespace fastchess