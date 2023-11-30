#pragma once

#include <string>

#include <cli.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <types/engine_config.hpp>
#include <types/enums.hpp>
#include <types/match_data.hpp>
#include <types/stats.hpp>

namespace fast_chess {

class Tournament;  // forward declaration

using pair_config = std::pair<fast_chess::EngineConfiguration, fast_chess::EngineConfiguration>;

/// @brief Interface for outputting current tournament state to the user.
class IOutput {
   public:
    IOutput()          = default;
    virtual ~IOutput() = default;

    /// @brief Interval output. Get's displayed every n `ratinginterval`.
    /// @param sprt
    /// @param stats
    /// @param first
    /// @param second
    /// @param current_game_count
    virtual void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                               const std::string& second, int current_game_count) {
        std::cout << "--------------------------------------------------\n";
        printElo(stats, first, second, current_game_count);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n";
    };

    /// @brief Print current H2H elo stats.
    /// @param stats
    /// @param first
    /// @param second
    /// @param current_game_count
    virtual void printElo(const Stats& stats, const std::string& first, const std::string& second,
                          std::size_t current_game_count) = 0;

    /// @brief Print current SPRT stats.
    /// @param sprt
    /// @param stats
    virtual void printSprt(const SPRT& sprt, const Stats& stats) = 0;

    /// @brief Print game start.
    /// @param first
    /// @param second
    /// @param current_game_count
    /// @param max_game_count
    virtual void startGame(const pair_config& configs, std::size_t current_game_count,
                           std::size_t max_game_count) = 0;

    /// @brief Print game end.
    /// @param stats
    /// @param first
    /// @param second
    /// @param annotation
    /// @param id
    virtual void endGame(const pair_config& configs, const Stats& stats,
                         const std::string& annotation, std::size_t id) = 0;

    /// @brief Print tournament end.
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