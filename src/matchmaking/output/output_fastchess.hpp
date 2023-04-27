#pragma once

#include "../../elo.hpp"
#include "../../logger.hpp"
#include "output_factory.hpp"

namespace fast_chess {

class Fastchess : public Output {
   public:
    void printInterval(const Stats& stats, const std::string& first, const std::string& second,
                       int total) override {
        Logger::cout("Interval");
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
        std::cout << ss.str();
    }

    void printElo() override { Logger::cout("Elo"); }

    void startGame() override { Logger::cout("Start game"); }

    void endGame() override { Logger::cout("End game"); }
};

}  // namespace fast_chess
