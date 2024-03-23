#pragma once

#include <matchmaking/elo/elo.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

class Cutechess : public IOutput {
   public:
    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                       const std::string& second, int current_game_count) override {
        printElo(stats, first, second, current_game_count);
        printSprt(sprt, stats);
    };

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  std::size_t current_game_count) override {
        const Elo elo(stats.wins, stats.losses, stats.draws);

        std::stringstream ss;
        ss << "Score of "                                                //
           << first                                                      //
           << " vs "                                                     //
           << second                                                     //
           << ": "                                                       //
           << stats.wins                                                 //
           << " - "                                                      //
           << stats.losses                                               //
           << " - "                                                      //
           << stats.draws                                                //
           << " ["                                                       //
           << Elo::getScoreRatio(stats.wins, stats.losses, stats.draws)  //
           << "] "                                                       //
           << current_game_count                                         //
           << "\n";

        ss << "Elo difference: "                                        //
           << elo.getElo()                                              //
           << ", "                                                      //
           << "LOS: "                                                   //
           << Elo::getLos(stats.wins, stats.losses)                     //
           << ", "                                                      //
           << "DrawRatio: "                                             //
           << Elo::getDrawRatio(stats.wins, stats.losses, stats.draws)  //
           << "\n";

        std::cout << ss.str() << std::flush;
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isValid()) {
            std::stringstream ss;

            ss << "LLR: " << std::fixed << std::setprecision(2)
               << sprt.getLLR(stats.wins, stats.draws, stats.losses) << " " << sprt.getBounds()
               << " " << sprt.getElo() << "\n";
            std::cout << ss.str() << std::flush;
        }
    };

    void startGame(const pair_config& configs, std::size_t current_game_count,
                   std::size_t max_game_count) override {
        std::stringstream ss;

        ss << "Started game "      //
           << current_game_count   //
           << " of "               //
           << max_game_count       //
           << " ("                 //
           << configs.first.name   //
           << " vs "               //
           << configs.second.name  //
           << ")"                  //
           << "\n";

        std::cout << ss.str() << std::flush;
    }

    void endGame(const pair_config& configs, const Stats& stats, const std::string& annotation,
                 std::size_t id) override {
        std::stringstream ss;

        ss << "Finished game "     //
           << id                   //
           << " ("                 //
           << configs.first.name   //
           << " vs "               //
           << configs.second.name  //
           << "): "                //
           << formatStats(stats)   //
           << " {"                 //
           << annotation           //
           << "}"                  //
           << "\n";

        std::cout << ss.str() << std::flush;
    }

    void endTournament() override { std::cout << "Tournament finished" << std::endl; }
};

}  // namespace fast_chess
