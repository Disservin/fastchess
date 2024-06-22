#pragma once

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <elo/elo_pentanomial.hpp>
#include <elo/elo_wdl.hpp>
#include <engine/uci_engine.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

class Fastchess : public IOutput {
   public:
    Fastchess(bool report_penta = true) : report_penta_(report_penta) {}

    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first, const std::string& second,
                       const std::pair<engine::UciEngine, engine::UciEngine>& engines,
                       const std::string& book) override {
        std::cout << "--------------------------------------------------\n" << std::flush;
        printElo(stats, first, second, engines, book);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n" << std::flush;
    };

    void printResult(const Stats&, const std::string&, const std::string&) override {
        // do nothing
    }

    void printElo(const Stats& stats, const std::string& first, const std::string& second,
                  const std::pair<engine::UciEngine, engine::UciEngine>& engines, const std::string& book) override {
        // int movestogo1       = limit1.tc.moves;
        // int movestogo2       = limit2.tc.moves;
        // int fixed_time1      = limit1.tc.fixed_time;
        // int fixed_time2      = limit2.tc.fixed_time;
        // int time1            = limit1.tc.time;
        // int time2            = limit2.tc.time;
        // int inc1             = limit1.tc.increment;
        // int inc2             = limit2.tc.increment;
        // int nodes1           = limit1.nodes;
        // int nodes2           = limit2.nodes;
        // int plies1           = limit1.plies;
        // int plies2           = limit2.plies;
        // std::string threads1 = "{}";
        // std::string threads2 = "{}";
        // std::string hash1    = "{}";
        // std::string hash2    = "{}";
        // auto hash1it =
        //     std::find_if(options1.begin(), options1.end(),
        //                  [](const std::pair<std::string, std::string>& element) { return element.first == "Hash"; });
        // auto hash2it =
        //     std::find_if(options2.begin(), options2.end(),
        //                  [](const std::pair<std::string, std::string>& element) { return element.first == "Hash"; });
        // auto threads1it =
        //     std::find_if(options1.begin(), options1.end(),
        //                  [](const std::pair<std::string, std::string>& element) { return element.first == "Threads";
        //                  });
        // auto threads2it =
        //     std::find_if(options2.begin(), options2.end(),
        //                  [](const std::pair<std::string, std::string>& element) { return element.first == "Threads";
        //                  });
        // if (hash1it != options1.end()) hash1 = hash1it->second;
        // if (hash2it != options2.end()) hash2 = hash2it->second;
        // if (threads1it != options1.end()) threads1 = threads1it->second;
        // if (threads2it != options2.end()) threads2 = threads2it->second;

        std::unique_ptr<elo::EloBase> elo;

        if (report_penta_) {
            elo = std::make_unique<elo::EloPentanomial>(stats);
        } else {
            elo = std::make_unique<elo::EloWDL>(stats);
        }

        auto timeFirst  = getTime(engines.first);
        auto timeSecond = getTime(engines.second);
        auto tc         = timeFirst == timeSecond ? timeFirst : fmt::format("{} - {}", timeFirst, timeSecond);

        auto threadsFirst  = getThreads(engines.first);
        auto threadsSecond = getThreads(engines.second);
        auto threads =
            threadsFirst == threadsSecond ? threadsFirst : fmt::format("{} - {}", threadsFirst, threadsSecond);

        auto hashFirst  = getHash(engines.first);
        auto hashSecond = getHash(engines.second);

        auto hash = hashFirst == hashSecond ? hashFirst : fmt::format("{} - {}", hashFirst, hashSecond);

        const auto games       = stats.wins + stats.losses + stats.draws;
        const auto points      = stats.wins + 0.5 * stats.draws;
        const auto pointsRatio = points / games * 100;

        const auto pairsRatio =
            static_cast<double>(stats.penta_WW + stats.penta_WD) / (stats.penta_LD + stats.penta_LL);

        const auto bookname = book.substr(book.find_last_of("/\\") + 1);

        // engine, engine2, tc, threads, hash, book
        auto line1 = fmt::format("Results of {} vs {} ({}, {}, {}, {}):", first, second, tc, threads, hash, book);
        auto line2 = fmt::format("Elo: {}, nElo: {}", elo->getElo(), elo->nElo());
        auto line3 =
            fmt::format("LOS: {}, DrawRatio: {}, PairsRatio: {:.2f}", elo->los(), elo->drawRatio(stats), pairsRatio);
        auto line4 = fmt::format("Games: {}, Wins: {}, Losses: {}, Draws: {}, Points: {:.1f} ({:.2f} %)", games,
                                 stats.wins, stats.losses, stats.draws, points, pointsRatio);
        auto line5 = fmt::format("Ptnml(0-2): [{}, {}, {}, {}, {}]", stats.penta_LL, stats.penta_LD,
                                 stats.penta_WL + stats.penta_DD, stats.penta_WD, stats.penta_WW);

        auto lines = fmt::format("{}\n{}\n{}\n{}\n{}\n{}", line1, line2, line3, line4, line5);

        // std::stringstream ss;
        // ss << first   //
        //    << " vs "  //
        //    << second  //
        //    << " (";

        // if (movestogo1 != 0) ss << movestogo1 << "moves" << "/";

        // if (time1 > 0)
        //     ss << time1 / 1000.0 << "s";
        // else if (fixed_time1 > 0)
        //     ss << fixed_time1 / 1000.0 << "s/move";
        // else if (plies1 > 0)
        //     ss << plies1 << "plies";
        // else if (nodes1 > 0)
        //     ss << nodes1 << "nodes";

        // if (inc1 != 0) ss << "+" << inc1 / 1000.0 << "s";

        // if (time1 != time2 || movestogo1 != movestogo2 || inc1 != inc2 || fixed_time1 != fixed_time2 ||
        //     plies1 != plies2 || nodes1 != nodes2) {
        //     ss << " - ";

        //     if (movestogo2 != 0) ss << movestogo2 << "moves" << "/";

        //     if (time2 > 0)
        //         ss << time2 / 1000.0 << "s";
        //     else if (fixed_time2 > 0)
        //         ss << fixed_time2 / 1000.0 << "s/move";
        //     else if (plies2 > 0)
        //         ss << plies2 << "plies";
        //     else if (nodes2 > 0)
        //         ss << nodes2 << "nodes";

        //     if (inc2 != 0) ss << "+" << inc2 / 1000.0 << "s";
        // }

        // ss << ", " << threads1 << "t";

        // if (threads1 != threads2) {
        //     ss << " - " << threads2 << "t";
        // }

        // ss << ", " << hash1 << "MB";

        // if (hash1 != hash2) {
        //     ss << " - " << hash2 << "MB";
        // }

        // std::size_t pos      = book.find_last_of("/\\");
        // std::string bookname = book;
        // if (pos != std::string::npos) {
        //     bookname = book.substr(pos + 1);
        // }

        // if (!book.empty()) {
        //     ss << ", " << bookname;
        // }

        // ss << "):\n"
        //    << "Elo: "        //
        //    << elo->getElo()  //
        //    << ", nElo: "     //
        //    << elo->nElo()    //
        //    << "\n";          //

        // ss << "LOS: "                 //
        //    << elo->los()              //
        //    << ", DrawRatio: "         //
        //    << elo->drawRatio(stats);  //

        // const double pairsRatio =
        //     static_cast<double>(stats.penta_WW + stats.penta_WD) / (stats.penta_LD + stats.penta_LL);
        // if (report_penta_) {
        //     ss << ", PairsRatio: " << std::fixed << std::setprecision(2) << pairsRatio << "\n";
        // } else {
        //     ss << "\n";
        // }

        // const double points = stats.wins + 0.5 * stats.draws;
        // ss << "Games: "                                                                                       //
        //    << stats.wins + stats.losses + stats.draws                                                         //
        //    << ", Wins: "                                                                                      //
        //    << stats.wins                                                                                      //
        //    << ", Losses: "                                                                                    //
        //    << stats.losses                                                                                    //
        //    << ", Draws: "                                                                                     //
        //    << stats.draws                                                                                     //
        //    << std::fixed << std::setprecision(1) << ", Points: "                                              //
        //    << points                                                                                          //
        //    << " ("                                                                                            //
        //    << std::fixed << std::setprecision(2) << points / (stats.wins + stats.losses + stats.draws) * 100  //
        //    << " %)\n";

        // if (report_penta_) {
        //     ss << "Ptnml(0-2): "
        //        << "[" << stats.penta_LL << ", " << stats.penta_LD << ", " << stats.penta_WL + stats.penta_DD << ", "
        //        << stats.penta_WD << ", " << stats.penta_WW << "]\n";
        // }

        std::cout << lines << std::flush;
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
    std::string getTime(const engine::UciEngine& engine) {
        if (engine.getConfig().limit.tc.time > 0) {
            return fmt::format("{:.2f}{}", engine.getConfig().limit.tc.time / 1000.0,
                               engine.getConfig().limit.tc.increment > 0
                                   ? fmt::format("+{:.2f}", engine.getConfig().limit.tc.increment / 1000.0)
                                   : "");
        } else if (engine.getConfig().limit.tc.fixed_time > 0) {
            return fmt::format("{:.2f}move", engine.getConfig().limit.tc.fixed_time / 1000.0);
        } else if (engine.getConfig().limit.plies > 0) {
            return fmt::format("{} plies", engine.getConfig().limit.plies);
        } else if (engine.getConfig().limit.nodes > 0) {
            return fmt::format("{} nodes", engine.getConfig().limit.nodes);
        }

        return "";
    }

    std::string getThreads(const engine::UciEngine& engine) {
        return fmt::format("{}", engine.getOption("Threads").value_or("1"));
    }

    std::string getHash(const engine::UciEngine& engine) {
        return fmt::format("{}", engine.getOption("Hash").value_or("NaN"));
    }

    bool report_penta_;
};

}  // namespace fast_chess
