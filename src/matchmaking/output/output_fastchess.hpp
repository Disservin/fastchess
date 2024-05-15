#pragma once

#include <elo/elo_pentanomial.hpp>
#include <elo/elo_wdl.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

class Fastchess : public IOutput {
   public:
    Fastchess(bool report_penta = true) : report_penta_(report_penta) {}

    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first,
                       const std::string& second) override {
        std::cout << "--------------------------------------------------\n" << std::flush;
        printElo(stats, first, second);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n" << std::flush;
    };

    void printResult(const Stats&, const std::string&, const std::string&) override {
        //do nothing
    }

    void printElo(const Stats& stats, const std::string& first,
                  const std::string& second) override {
        std::unique_ptr<elo::EloBase> elo;

        if (report_penta_) {
            elo = std::make_unique<elo::EloPentanomial>(stats);
        } else {
            elo = std::make_unique<elo::EloWDL>(stats);
        }

        std::stringstream ss;
        ss << "Results of "  //
           << first          //
           << " vs "         //
           << second         //
           << ": \n";

        ss << "Elo: "        //
           << elo->getElo()  //
           << ", nElo: "     //
           << elo->nElo()    //
           << "\n";          //

        ss << "LOS: "                 //
           << elo->los()              //
           << ", DrawRatio: "         //
           << elo->drawRatio(stats);  //

        const double pairsRatio = static_cast<double>(stats.penta_WW + stats.penta_WD) /
                                  (stats.penta_LD + stats.penta_LL);
        if (report_penta_) {
            ss << ", PairsRatio: " << std::fixed << std::setprecision(2) << pairsRatio << "\n";
        } else {
            ss << "\n";
        }

        const double points = stats.wins + 0.5 * stats.draws;
        ss << "Games: "                                           //
           << stats.wins + stats.losses + stats.draws             //
           << ", Wins: "                                          //
           << stats.wins                                          //
           << ", Losses: "                                        //
           << stats.losses                                        //
           << ", Draws: "                                         //
           << stats.draws                                         //
           << std::fixed << std::setprecision(1) << ", Points: "  //
           << points                                              //
           << " ("                                                //
           << std::fixed << std::setprecision(2)
           << points / (stats.wins + stats.losses + stats.draws) * 100  //
           << " %)\n";

        if (report_penta_) {
            ss << "Ptnml(0-2): " << "[" << stats.penta_LL << ", " << stats.penta_LD << ", "
               << stats.penta_WL + stats.penta_DD << ", " << stats.penta_WD << ", "
               << stats.penta_WW << "]\n";
        }
        std::cout << ss.str() << std::flush;
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isValid()) {
            double llr = sprt.getLLR(stats, report_penta_);

            std::stringstream ss;

            ss << "LLR: " << std::fixed << std::setprecision(2) << llr  //
               << " " << sprt.getBounds()                               //
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
