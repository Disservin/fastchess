#pragma once

#include <string>

#include <cli.hpp>
#include <enums.hpp>
#include <matchmaking/types/match_data.hpp>
#include <matchmaking/types/stats.hpp>
#include <sprt.hpp>

namespace fast_chess {

class Tournament;  // forward declaration

class IOutput {
   public:
    IOutput() = default;
    virtual ~IOutput() = default;

    [[nodiscard]] virtual OutputType getType() const = 0;

    virtual void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                               const std::string& second, int current_game_count) {
        std::cout << "--------------------------------------------------\n";
        printElo(stats, first, second, current_game_count);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n";
    };

    virtual void printElo(const Stats& stats, const std::string& first, const std::string& second,
                          int current_game_count) = 0;

    virtual void printSprt(const SPRT& sprt, const Stats& stats) = 0;

    virtual void startGame(const std::string& first, const std::string& second,
                           int current_game_count, int max_game_count) = 0;

    virtual void endGame(const Stats& stats, const std::string& first, const std::string& second,
                         const std::string& annotation, int id) = 0;

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