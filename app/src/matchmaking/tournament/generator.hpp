#pragma once

#include <config/config.hpp>
#include <optional>
#include <tuple>
#include <vector>

#include <book/opening_book.hpp>

namespace fastchess {

class MatchGenerator {
   public:
    MatchGenerator() : i(0), j(1), k(0), g(0), engine_configs_size(config::EngineConfigs.get().size()) {
        opening_book_ = nullptr;
    }

    void setup(book::OpeningBook* opening_book, int initial_matchcount) {
        opening_book_ = opening_book;
        offset_       = initial_matchcount / config::TournamentConfig.get().games;
    }

    std::optional<std::tuple<std::size_t, std::size_t, std::size_t, int, std::optional<std::size_t>>> next() {
        if (i >= engine_configs_size || j >= engine_configs_size) return std::nullopt;

        if (g == 0) {
            // Fetch a new opening only once per round
            opening = opening_book_->fetchId();
        }

        auto match = std::make_tuple(i, j, k, g, opening);

        advance();

        return match;
    }

   private:
    std::optional<std::size_t> opening;
    book::OpeningBook* opening_book_;
    std::size_t i, j, k, g;
    std::size_t engine_configs_size;
    int offset_;

    int games  = config::TournamentConfig.get().games;
    int rounds = config::TournamentConfig.get().rounds;

    void advance() {
        if (++g >= games) {
            g = 0;
            if (++k >= rounds) {
                k = offset_;
                if (++j >= engine_configs_size) {
                    j = ++i + 1;
                }
            }
        }
    }
};

}  // namespace fastchess