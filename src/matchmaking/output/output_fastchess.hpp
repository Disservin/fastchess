#pragma once

#include <matchmaking/elo/elo_logistic.hpp>
#include <matchmaking/elo/elo_norm.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

class Fastchess : public IOutput {
   public:
    Fastchess(bool report_penta = true) : report_penta_(report_penta) {}

    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                       const std::string& second, int current_game_count) override {
        std::cout << "--------------------------------------------------\n";
        printElo(stats, first, second, current_game_count);
        printSprt(sprt, stats);
        if (report_penta_) {
            printPenta(stats);
        }
        std::cout << "--------------------------------------------------\n";
    };

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  std::size_t current_game_count) override {
        std::unique_ptr<EloBase> elo;

        if (report_penta_) {
            elo = std::make_unique<EloNormalized>(stats);
        } else {
            elo = std::make_unique<EloLogistic>(stats);
        }

        std::stringstream ss;
        ss << "Score of "                //
           << first                      //
           << " vs "                     //
           << second                     //
           << ": "                       //
           << stats.wins                 //
           << "W - "                     //
           << stats.losses               //
           << "L - "                     //
           << stats.draws                //
           << "D ["                      //
           << elo->getScoreRatio(stats)  //
           << "] "                       //
           << current_game_count         //
           << "\n";

        if (report_penta_) {
            ss << "Elo difference: "        //
               << elo->getElo()             //
               << ", "                      //
               << "nElo difference: "       //
               << elo->getnElo()            //
               << ", "                      //
               << "LOS: "                   //
               << elo->getLos(stats)        //
               << ", "                      //
               << "PairDrawRatio: "         //
               << elo->getDrawRatio(stats)  //
               << "\n";
        } else {
            ss << "Elo difference: "        //
               << elo->getElo()             //
               << ", "                      //
               << "nElo difference: "       //
               << elo->getnElo()            //
               << ", "                      //
               << "LOS: "                   //
               << elo->getLos(stats)        //
               << ", "                      //
               << "DrawRatio: "             //
               << elo->getDrawRatio(stats)  //
               << "\n";
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
    };

    static void printPenta(const Stats& stats) {
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

   private:
    bool report_penta_;
};

}  // namespace fast_chess
