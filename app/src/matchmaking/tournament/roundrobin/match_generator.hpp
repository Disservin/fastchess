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
    struct Pairing {
        int round_id;
        int pairing_id;
        int game_id;
        std::optional<std::size_t> opening_id;
        int player1;
        int player2;

        Pairing(int roundId, int gameId, int p1, int p2)
            : round_id(roundId), game_id(gameId), player1(p1), player2(p2) {}
    };

    MatchGenerator(book::OpeningBook* opening_book, int players, int rounds, int games, int played_games)
        : opening_book_(opening_book),
          n_players(players),
          n_rounds(rounds),
          n_games_per_round(games),
          current_round(1),
          game_counter(0),
          player1(0),
          player2(1),
          games_per_pair(0) {
        current_round = (played_games / games) + 1;

        if (n_players < 2 || n_rounds < 1 || n_games_per_round < 1) {
            throw std::invalid_argument("Invalid number of players, rounds, or games per round");
        }
    }

    // Function to generate the next game pairing
    std::optional<Pairing> next() {
        Pairing nextGame = Pairing(0, 0, 0, 0);

        // Check if the current round is beyond the number of rounds
        if (current_round > n_rounds) {
            // No more rounds left
            return std::nullopt;
        }

        if (games_per_pair == 0) {
            opening = opening_book_->fetchId();
        }

        nextGame.round_id   = current_round;
        nextGame.game_id    = ++game_counter;
        nextGame.player1    = player1;
        nextGame.player2    = player2;
        nextGame.pairing_id = pairCounter;
        nextGame.opening_id = opening;

        // Increment the games between the current player1 and player2
        games_per_pair++;

        // If we've exhausted the games between the current player pair, move to the next pair
        if (games_per_pair >= n_games_per_round) {
            games_per_pair = 0;

            player2++;
            pairCounter++;

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

        return nextGame;
    }

   private:
    book::OpeningBook* opening_book_;
    std::optional<std::size_t> opening;
    int n_players;
    int n_rounds;
    int n_games_per_round;
    int current_round;
    int game_counter;
    int player1;
    int player2;
    int games_per_pair;
    int pairCounter;
};

}  // namespace fastchess
