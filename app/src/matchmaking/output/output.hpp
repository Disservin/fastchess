#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <matchmaking/scoreboard.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <matchmaking/stats.hpp>
#include <types/engine_config.hpp>
#include <types/enums.hpp>
#include <types/match_data.hpp>

namespace fastchess {

namespace engine {
class UciEngine;
}

using engines = std::pair<const engine::UciEngine&, const engine::UciEngine&>;

// Interface for outputting current tournament state to the user.
class IOutput {
   public:
    IOutput()          = default;
    virtual ~IOutput() = default;

    // Interval output. Get's displayed every n `ratinginterval`.
    virtual void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                               const std::string& second, const engines& engines, const std::string& book,
                               ScoreBoard& scoreboard) {
        std::cout << "--------------------------------------------------\n";
        printElo(stats, first, second, engines, book, scoreboard);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n";
    };

    // Print current H2H score result stats.
    virtual void printResult(const Stats& stats, const std::string& first, const std::string& second) = 0;

    // Print current H2H elo stats.
    virtual std::string printElo(const Stats& stats, const std::string& first, const std::string& second,
                                 const engines& engines, const std::string& book, ScoreBoard& scoreboard) = 0;

    // Print current SPRT stats.
    virtual std::string printSprt(const SPRT& sprt, const Stats& stats) = 0;

    // Print game start.
    virtual void startGame(const GamePair<EngineConfiguration, EngineConfiguration>& configs,
                           std::size_t current_game_count, std::size_t max_game_count) = 0;

    // Print game end.
    virtual void endGame(const GamePair<EngineConfiguration, EngineConfiguration>& configs, const Stats& stats,
                         const std::string& annotation, std::size_t id) = 0;

    // Print tournament end.
    virtual void endTournament(std::string_view terminationMessage = "") = 0;

    virtual OutputType getType() const = 0;

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

}  // namespace fastchess
