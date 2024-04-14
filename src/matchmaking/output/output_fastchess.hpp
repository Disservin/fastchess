#pragma once

#include <matchmaking/elo/elo_wdl.hpp>
#include <matchmaking/elo/elo_pentanomial.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

class Fastchess : public IOutput {
   public:
    Fastchess(bool report_penta = true) : report_penta_(report_penta) {}

    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                       const std::string& second, int current_game_count) override {
        std::cout << "--------------------------------------------------\n";
        printSprt(sprt, stats);
        printElo(stats, first, second, current_game_count);
        std::cout << "--------------------------------------------------\n";
    };

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  std::size_t current_game_count) override {
        std::unique_ptr<EloBase> elo;

        if (report_penta_) {
            elo = std::make_unique<EloPentanomial>(stats);
        } else {
            elo = std::make_unique<EloWDL>(stats);
        }

        std::stringstream ss;
        ss << "Results of "           //
           << first                   //
           << " vs "                  //
           << second                  //
           << ": \n";                 

        ss << "Elo: "                 //
           << elo->getElo()           //
           << " nElo: "               //
           << elo->getnElo()          //
           << "\n";                   //
   
        ss << "LOS: "                 //
           << elo->los(stats)         //
           << " DrawRatio: "          //
           << elo->drawRatio(stats);  //

        const double pairsRatio = static_cast<double>(stats.penta_WW + stats.penta_WD) / (stats.penta_LD + stats.penta_LL);
        if (report_penta_){
           ss << "PairsRatio: "
              << pairsRatio
              << "\n";
        } else {
           ss << "\n";
        }

         ss << "Games: "              //
            << current_game_count     //
            << " W: "                 //
            << stats.wins             //
            << " L: "                 //
            << stats.losses           //
            << " D: "                 //
            << stats.draws            //
            << " Points: "            //
            << stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD + stats.penta_LD + stats.penta_LL          //
            << " ("                   //
            << std::stod(elo->scoreRatio(stats)) * 100 //
            << "%)\n";

         if (report_penta_){
            ss << "Ptnml(0-2): "
               << "["
               << stats.penta_WW
               << ", "
               << stats.penta_WD
               << ", "
               << stats.penta_WL + stats.penta_DD
               << ", "
               << stats.penta_LD
               << ", "
               << stats.penta_LL
               << "]\n";
         }
        std::cout << ss.str() << std::flush;
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isValid()) {
            std::stringstream ss;
            double llr;
            if (report_penta_)
                llr = sprt.getLLR(stats.penta_WW, stats.penta_WD, stats.penta_WL, stats.penta_DD,
                                  stats.penta_LD, stats.penta_LL);
            else
                llr = sprt.getLLR(stats.wins, stats.draws, stats.losses);
            ss << "LLR: " << std::fixed << std::setprecision(2) << llr << " " << sprt.getBounds()
               << " " << sprt.getElo() << "\n";
            std::cout << ss.str() << std::flush;
        }
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

   private:
    bool report_penta_;
};

}  // namespace fast_chess
