#pragma once

#include <config/config.hpp>
#include <optional>
#include <tuple>
#include <vector>

#include <book/opening_book.hpp>

namespace fastchess {

/**
 * Generate matches for a round-robin tournament.
 * Everytime a new match is required the next() function is called.
 */
class MatchGenerator {
   public:
    MatchGenerator(book::OpeningBook* opening_book, int initial_matchcount)
        : ecidx_one(0), ecidx_two(1), round_id(0), pair_id(0), engine_configs_size(config::EngineConfigs.get().size()) {
        opening_book_ = opening_book;
        offset_       = initial_matchcount / games;
        round_id      = offset_;
    }

    std::optional<std::tuple<std::size_t, std::size_t, std::size_t, int, std::optional<std::size_t>>> next() {
        if (ecidx_one >= engine_configs_size || ecidx_two >= engine_configs_size) return std::nullopt;

        if (pair_id == 0) {
            // Fetch a new opening only once per round
            opening = opening_book_->fetchId();
        }

        auto match = std::make_tuple(ecidx_one, ecidx_two, round_id, pair_id, opening);

        advance();

        return match;
    }

   private:
    std::optional<std::size_t> opening;
    book::OpeningBook* opening_book_;

    // 1 - 2
    const std::size_t games  = config::TournamentConfig.get().games;
    const std::size_t rounds = config::TournamentConfig.get().rounds;

    // Engine configuration indices
    std::size_t ecidx_one, ecidx_two;
    std::size_t round_id;

    // 0 - 1
    std::size_t pair_id;
    std::size_t engine_configs_size;

    int offset_;

    void advance() {
        if (++pair_id >= games) {
            pair_id = 0;

            if (++round_id >= rounds) {
                round_id = offset_;

                if (++ecidx_two >= engine_configs_size) {
                    ecidx_two = ++ecidx_one + 1;
                }
            }
        }
    }
};

}  // namespace fastchess
