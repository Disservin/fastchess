#pragma once

#include <core/config/config.hpp>
#include <optional>
#include <tuple>
#include <vector>

#include <game/book/opening_book.hpp>

namespace fastchess {

/**
 * Generate matches for a round-robin tournament.
 * Everytime a new match is required the next() function is called.
 */
class MatchGenerator {
   public:
    struct Pairing {
        std::size_t round_id;
        std::size_t pairing_id;
        std::size_t game_id;
        std::optional<std::size_t> opening_id;
        std::size_t player1;
        std::size_t player2;
    };

    MatchGenerator(book::OpeningBook* opening_book, std::size_t players, std::size_t rounds, std::size_t games,
                   std::size_t played_games)
        : opening_book_(opening_book),
          n_players(players),
          n_rounds(rounds),
          n_games_per_round(games),
          current_round(1),
          game_counter(played_games),
          player1(0),
          player2(1),
          games_per_pair(0),
          pair_counter(played_games / games) {
        current_round = (played_games / games) + 1;

        if (n_players < 2 || n_rounds < 1 || n_games_per_round < 1) {
            throw std::invalid_argument("Invalid number of players, rounds, or games per round");
        }
    }

    // Function to generate the next game pairing
    std::optional<Pairing> next() {
        Pairing next_game;

        // Check if the current round is beyond the number of rounds
        if (current_round > n_rounds) {
            // No more rounds left
            return std::nullopt;
        }

        if (games_per_pair == 0) {
            opening = opening_book_->fetchId();
        }

        next_game.round_id   = current_round;
        next_game.game_id    = ++game_counter;
        next_game.player1    = player1;
        next_game.player2    = player2;
        next_game.pairing_id = pair_counter;
        next_game.opening_id = opening;

        // Increment the games between the current player1 and player2
        games_per_pair++;

        // If we've exhausted the games between the current player pair, move to the next pair
        if (games_per_pair >= n_games_per_round) {
            games_per_pair = 0;

            player2++;
            pair_counter++;

            if (player2 >= n_players) {
                player1++;
                player2 = player1 + 1;
            }

            // If we've exhausted all pairs for this round, move to the next round
            if (player1 >= n_players - 1) {
                current_round++;

                player1 = 0;
                player2 = 1;
            }
        }

        return next_game;
    }

   private:
    book::OpeningBook* opening_book_;
    std::optional<std::size_t> opening;
    std::size_t n_players;
    std::size_t n_rounds;
    std::size_t n_games_per_round;
    std::size_t current_round;
    std::size_t game_counter;
    std::size_t player1;
    std::size_t player2;
    std::size_t games_per_pair;
    std::size_t pair_counter;
};

}  // namespace fastchess
