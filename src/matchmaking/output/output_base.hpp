#pragma once

#include <string>

#include "../../options.hpp"
#include "../../sprt.hpp"
#include "../types/match_data.hpp"
#include "../types/stats.hpp"

namespace fast_chess {

class Tournament;  // forward declaration

enum class OutputType {
    FASTCHESS,
    CUTECHESS,
};

class Output {
   public:
    Output() = default;
    virtual ~Output() = default;

    [[nodiscard]] virtual OutputType getType() const = 0;

    virtual void printInterval(const Stats& stats, const std::string& first,
                               const std::string& second, int total) = 0;

    [[nodiscard]] virtual std::string printElo(const Stats& stats, const std::string& first,
                                               const std::string& second, int total) = 0;

    virtual void startGame(const std::string& first, const std::string& second, int current,
                           int total) = 0;

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