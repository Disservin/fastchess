#pragma once

#include <string>

#include <cli/cli.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <types/engine_config.hpp>
#include <types/enums.hpp>
#include <types/match_data.hpp>
#include <types/stats.hpp>

namespace fast_chess {

using pair_config = std::pair<EngineConfiguration, EngineConfiguration>;

// Interface for outputting current tournament state to the user.
class IOutput {
   public:
    IOutput()          = default;
    virtual ~IOutput() = default;

    // Interval output. Get's displayed every n `ratinginterval`.
    virtual void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                               const std::string& second, 
                               std::vector<std::pair<std::string, std::string>>& options1, 
                               std::vector<std::pair<std::string, std::string>>& options2,
                               Limit& limit1, Limit& limit2, const std::string& book) {
        std::cout << "--------------------------------------------------\n";
        printElo(stats, first, second, options1, options2, limit1, limit2, book);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n";
    };

    // Print current H2H score result stats.
    virtual void printResult(const Stats& stats, const std::string& first, const std::string& second) = 0;

    // Print current H2H elo stats.
    virtual void printElo(const Stats& stats, const std::string& first, const std::string& second,
                          std::vector<std::pair<std::string, std::string>>& options1, 
                          std::vector<std::pair<std::string, std::string>>& options2,
                          Limit& limit1, Limit& limit2, const std::string& book) = 0;

    // Print current SPRT stats.
    virtual void printSprt(const SPRT& sprt, const Stats& stats) = 0;

    // Print game start.
    virtual void startGame(const pair_config& configs, std::size_t current_game_count, std::size_t max_game_count) = 0;

    // Print game end.
    virtual void endGame(const pair_config& configs, const Stats& stats, const std::string& annotation,
                         std::size_t id) = 0;

    // Print tournament end.
    virtual void endTournament() = 0;

    [[nodiscard]] static std::string formatStats(const Stats& stats) {
        if (stats.wins) {
            return "1-0";
        }
        if (stats.losses) {
            return "0-1";
        }
        return "1/2-1/2";
    };
};

}  // namespace fast_chess
