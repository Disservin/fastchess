#pragma once

#include <matchmaking/elo/elo.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess {

class Fastchess : public IOutput {
   public:
    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                       const std::string& second, int current_game_count) override {
        std::cout << "--------------------------------------------------\n";
        printElo(stats, first, second, current_game_count);
        printSprt(sprt, stats);
        printPenta(stats);
        std::cout << "--------------------------------------------------\n";
    };

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  std::size_t current_game_count) override {
        options::Tournament tournament
        if (tournament.report_penta == true){
           const Elo elo(stats.penta_WW, stats.penta_WD, stats.penta_WL, stats.penta_DD,
                         stats.penta_LD, stats.penta_LL);
   
           std::stringstream ss;
           ss << "Score of "   //
              << first         //
              << " vs "        //
              << second        //
              << ": "          //
              << stats.wins    //
              << "W - "        //
              << stats.losses  //
              << "L - "        //
              << stats.draws   //
              << "D ["         //
              << Elo::getScoreRatio(stats.penta_WW, stats.penta_WD, stats.penta_WL, stats.penta_DD,
                                    stats.penta_LD, stats.penta_LL)  //
              << "] "                                                //
              << current_game_count                                  //
              << "\n";
   
           ss << "Elo difference: "   //
              << elo.getElo()         //
              << ", "                 //
              << "nElo difference: "  //
              << elo.getnElo()        //
              << ", "                 //
              << "LOS: "              //
              << Elo::getLos(stats.penta_WW, stats.penta_WD, stats.penta_WL, stats.penta_DD,
                             stats.penta_LD, stats.penta_LL)  //
              << ", "                                         //
              << "PairDrawRatio: "                            //
              << Elo::getDrawRatio(stats.penta_WW, stats.penta_WD, stats.penta_WL, stats.penta_DD,
                                   stats.penta_LD, stats.penta_LL)  //
              << "\n";
   
           std::cout << ss.str() << std::flush;
        } else {
           const Elo elo(stats.wins, stats.losses, stats.draws);
   
           std::stringstream ss;
           ss << "Score of "   //
              << first         //
              << " vs "        //
              << second        //
              << ": "          //
              << stats.wins    //
              << "W - "        //
              << stats.losses  //
              << "L - "        //
              << stats.draws   //
              << "D ["         //
              << Elo::getScoreRatio(stats.wins, stats.losses, stats.draws)  //
              << "] "                                                //
              << current_game_count                                  //
              << "\n";
   
           ss << "Elo difference: "   //
              << elo.getElo()         //
              << ", "                 //
              << "nElo difference: "  //
              << elo.getnElo()        //
              << ", "                 //
              << "LOS: "              //
              << Elo::getLos(stats.wins, stats.losses, stats.draws)  //
              << ", "                                         //
              << "DrawRatio: "                            //
              << Elo::getDrawRatio(stats.wins, stats.losses, stats.draws)  //
              << "\n";
   
           std::cout << ss.str() << std::flush;
        }
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isValid()) {
            options::Tournament tournament
            if(tournament.report_penta == true) {
               std::stringstream ss;
   
               ss << "LLR: " << std::fixed << std::setprecision(2)
                  << sprt.getLLR(stats.penta_WW, stats.penta_WD, stats.penta_WL, stats.penta_DD,
                                 stats.penta_LD, stats.penta_LL)
                  << " " << sprt.getBounds() << " " << sprt.getElo() << "\n";
               std::cout << ss.str() << std::flush;
            } else {
               std::stringstream ss;
   
               ss << "LLR: " << std::fixed << std::setprecision(2)
                  << sprt.getLLR(stats.wins, stats.losses, stats.draws)
                  << " " << sprt.getBounds() << " " << sprt.getElo() << "\n";
               std::cout << ss.str() << std::flush;
            }
        }
    };

    static void printPenta(const Stats& stats) {
        options::tournament tournament
        if (tournament.report_penta == true){
           std::stringstream ss;
   
           ss << "Ptnml:   " << std::right << std::setw(7)  //
              << "LL" << std::right << std::setw(7)         //
              << "LD" << std::right << std::setw(7)         //
              << "DD/WL" << std::right << std::setw(7)      //
              << "WD" << std::right << std::setw(7)         //
              << "WW"
              << "\n"
              << "Distr:   " << std::right << std::setw(7)                      //
              << stats.penta_LL << std::right << std::setw(7)                   //
              << stats.penta_LD << std::right << std::setw(7)                   //
              << stats.penta_WL + stats.penta_DD << std::right << std::setw(7)  //
              << stats.penta_WD << std::right << std::setw(7)                   //
              << stats.penta_WW << "\n";
        } else {
           std::stringstream ss;
           ss << "";
        }
        std::stringstream ss;
        std::cout << ss.str() << std::flush;
    }

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
