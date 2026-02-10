#pragma once

#include <core/config/config.hpp>
#include <game/book/opening_book.hpp>
#include <matchmaking/tournament/schedule/base_scheduler.hpp>

namespace fastchess {

/**
 * Generate matches for a round-robin tournament.
 * Everytime a new match is required the next() function is called.
 */
class GauntletScheduler : public TournamentSchedulerBase {
   public:
    GauntletScheduler(book::OpeningBook* opening_book, std::size_t players, std::size_t rounds, std::size_t games,
                      std::size_t engine_seeds, std::size_t played_games = 0)
        : TournamentSchedulerBase(opening_book, players, rounds, games, played_games),
          n_seeds(std::min(engine_seeds, players - 1)) {}

    std::size_t total() const override {
        std::size_t n_pairings = n_seeds * n_players - n_seeds * (n_seeds + 1) / 2;
        return n_pairings * n_rounds * n_games_per_round;
    }

   protected:
    std::size_t getPlayer1Limit() const override { return n_seeds; }

   private:
    std::size_t n_seeds;
};
}  // namespace fastchess
