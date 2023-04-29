#pragma once

#include "../../elo.hpp"
#include "../../logger.hpp"
#include "output_factory.hpp"

namespace fast_chess {

class Fastchess : public Output {
   public:
    void printInterval(const Stats& stats, const std::string& first, const std::string& second,
                       int total) override {
        std::cout << printElo(stats, first, second, total);
    }

    std::string printElo(const Stats& stats, const std::string& first, const std::string& second,
                         int total) override {
        Elo elo(stats.wins, stats.losses, stats.draws);

        // clang-format off
        std::stringstream ss;
        ss  << "Score of " 
            << first 
            << " vs " 
            << second 
            << ": "
            << stats.wins 
            << " - " 
            << stats.losses
            << " - " 
            << stats.draws 
            << " [] " 
            << total
            << "\n";
        
        ss  << "Elo difference: " 
            << elo.getElo()
            << ", "
            << "LOS: "
            << elo.getLos(stats.wins, stats.losses)
            << ", "
            << "DrawRatio: "
            << elo.getDrawRatio(stats.wins, stats.losses, stats.draws)
            << "\n";
        // clang-format on
        return ss.str();
    }

    void startGame(const std::string& first, const std::string& second, int current,
                   int total) override {
        std::stringstream ss;

        // clang-format off
        ss << "Started game "
           << current
           << " of "
           << total
           << " ("
           << first
           << " vs "
           << second
           << ")"
           << "\n";
        // clang-format on

        std::cout << ss.str();
    }

    void endGame(const Stats& stats, const std::string& first, const std::string& second,
                 const std::string& annotation, int id) override {
        std::stringstream ss;

        // clang-format off
        ss << "Finished game "
           << id
           << " ("
           << first
           << " vs "
           << second
           << "): "
           << formatStats(stats)
           << " {"
           << annotation 
           << "}"
           << "\n";
        // clang-format on

        std::cout << ss.str();
    }

    virtual void endTournament() { std::cout << "Tournament finished" << std::endl; }
};

}  // namespace fast_chess
