#pragma once

#include <elo.hpp>
#include <logger.hpp>
#include <matchmaking/output/output.hpp>

namespace fast_chess {

class Cutechess : public IOutput {
   public:
    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                       const std::string& second, int current_game_count) override {
        printElo(stats, first, second, current_game_count);
        printSprt(sprt, stats);
    };

    [[nodiscard]] OutputType getType() const override { return OutputType::CUTECHESS; }

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  int current_game_count) override {
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
            << current_game_count
            << "\n";
        
        ss  << "Elo difference: " 
            << elo.getElo()
            << ", "
            << "LOS: "
            << Elo::getLos(stats.wins, stats.losses)
            << ", "
            << "DrawRatio: "
            << Elo::getDrawRatio(stats.wins, stats.losses, stats.draws)
            << "\n";
        // clang-format on
        std::cout << ss.str();
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isValid()) {
            std::stringstream ss;

            ss << "LLR: " << std::fixed << std::setprecision(2)
               << sprt.getLLR(stats.wins, stats.draws, stats.losses) << " " << sprt.getBounds()
               << " " << sprt.getElo() << "\n";
            std::cout << ss.str();
        }
    };

    void startGame(const std::string& first, const std::string& second, int current_game_count,
                   int max_game_count) override {
        std::stringstream ss;

        // clang-format off
        ss << "Started game "
           << current_game_count
           << " of "
           << max_game_count
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

    void endTournament() override { std::cout << "Tournament finished" << std::endl; }
};

}  // namespace fast_chess
