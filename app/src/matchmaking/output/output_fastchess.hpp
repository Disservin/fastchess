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
                       const std::string& second, 
                       std::vector<std::pair<std::string, std::string>>& options1, 
                       std::vector<std::pair<std::string, std::string>>& options2,
                       Limit& limit1, Limit& limit2, const std::string& book) override {
        std::cout << "--------------------------------------------------\n" << std::flush;
        printElo(stats, first, second, options1, options2, limit1, limit2, book);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n" << std::flush;
    };

    void printResult(const Stats&, const std::string&, const std::string&) override {
        // do nothing
    }

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  std::vector<std::pair<std::string, std::string>>& options1, 
                  std::vector<std::pair<std::string, std::string>>& options2,
                  Limit& limit1, Limit& limit2, const std::string& book) override {
        std::unique_ptr<elo::EloBase> elo;
        int movestogo1 = limit1.tc.moves;
        int movestogo2 = limit2.tc.moves;
        int fixed_time1 = limit1.tc.fixed_time;
        int fixed_time2 = limit2.tc.fixed_time;
        int tc1 = limit1.tc.time;
        int tc2 = limit2.tc.time;
        int nodes1 = limit1.nodes;
        int nodes2 = limit2.nodes;
        int plies1 = limit1.plies;
        int plies2 = limit2.plies;
        std::string threads1 = "{}";
        std::string threads2 = "{}";
        std::string hash1 = "{}";
        std::string hash2 = "{}";
        auto hash1it = std::find_if(options1.begin(), options1.end(),
                           [](const std::pair<std::string, std::string>& element) {
                               return element.first == "Hash";
                           });
        auto hash2it = std::find_if(options2.begin(), options2.end(),
                           [](const std::pair<std::string, std::string>& element) {
                               return element.first == "Hash";
                           });
        auto threads1it = std::find_if(options1.begin(), options1.end(),
                           [](const std::pair<std::string, std::string>& element) {
                               return element.first == "Threads";
                           });
        auto threads2it = std::find_if(options2.begin(), options2.end(),
                           [](const std::pair<std::string, std::string>& element) {
                               return element.first == "Threads";
                           });
        if (hash1it != options1.end()) hash1 = hash1it->second;
        if (hash2it != options2.end()) hash2 = hash2it->second;
        if (threads1it != options1.end()) threads1 = threads1it->second;
        if (threads2it != options2.end()) threads1 = threads2it->second;

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
           << " (";

        if (movestogo1 != 0)
            ss << movestogo1 << "/";

        ss << tc1 / 1000.0;

        if (inc1 != 0)
            ss << "+" << inc1 / 1000.0;
       
        if (tc1 != tc2 || movestogo1 != movestogo2 || inc1 != inc2) {
             ss << " - ";
           
            if (movestogo2 != 0)
                ss << movestogo2 << "/";
   
            ss << tc2 / 1000.0;
   
            if (inc2 != 0)
                ss << "+" << inc2 / 1000.0;
        }

        ss << "s, "
           << threads1
           << "t";
       
        if (threads1 != threads2) {
            ss << " - "
               << threads2
               << "t";
        }

        ss << ", "
           << hash1
           << "MB";

        if (hash1 != hash2) {
            ss << " - "
               << hash2
               << "MB";
        }
           
        if (!book.empty()) {
            ss << ", "
               << book;
        }
       
        ss << "):\n"
           << "Elo: "        //
           << elo->getElo()  //
           << ", nElo: "     //
           << elo->nElo()    //
           << "\n";          //

        ss << "LOS: "                 //
           << elo->los()              //
           << ", DrawRatio: "         //
           << elo->drawRatio(stats);  //

        const double pairsRatio =
            static_cast<double>(stats.penta_WW + stats.penta_WD) / (stats.penta_LD + stats.penta_LL);
        if (report_penta_) {
            ss << ", PairsRatio: " << std::fixed << std::setprecision(2) << pairsRatio << "\n";
        } else {
            ss << "\n";
        }

        const double points = stats.wins + 0.5 * stats.draws;
        ss << "Games: "                                                                                       //
           << stats.wins + stats.losses + stats.draws                                                         //
           << ", Wins: "                                                                                      //
           << stats.wins                                                                                      //
           << ", Losses: "                                                                                    //
           << stats.losses                                                                                    //
           << ", Draws: "                                                                                     //
           << stats.draws                                                                                     //
           << std::fixed << std::setprecision(1) << ", Points: "                                              //
           << points                                                                                          //
           << " ("                                                                                            //
           << std::fixed << std::setprecision(2) << points / (stats.wins + stats.losses + stats.draws) * 100  //
           << " %)\n";

        if (report_penta_) {
            ss << "Ptnml(0-2): "
               << "[" << stats.penta_LL << ", " << stats.penta_LD << ", " << stats.penta_WL + stats.penta_DD << ", "
               << stats.penta_WD << ", " << stats.penta_WW << "]\n";
        }
        std::cout << ss.str() << std::flush;
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isEnabled()) {
            double llr = sprt.getLLR(stats, report_penta_);

            std::stringstream ss;

            ss << "LLR: " << std::fixed << std::setprecision(2) << llr  //
               << " " << sprt.getBounds()                               //
               << " " << sprt.getElo() << "\n";

            std::cout << ss.str() << std::flush;
        }
    }

    void startGame(const pair_config& configs, std::size_t current_game_count, std::size_t max_game_count) override {
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
