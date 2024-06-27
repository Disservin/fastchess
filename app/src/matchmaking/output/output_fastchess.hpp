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
                       const engines& engines, const std::string& book) override {
        std::cout << "--------------------------------------------------\n" << std::flush;
        printElo(stats, first, second, engines, book);
        printSprt(sprt, stats);
        std::cout << "--------------------------------------------------\n" << std::flush;
    };

    void printResult(const Stats&, const std::string&, const std::string&) override {
        // do nothing
    }

    void printElo(const Stats& stats, const std::string& first, const std::string& second, const engines& engines,
                  const std::string& book) override {
        std::unique_ptr<elo::EloBase> elo;

        if (report_penta_) {
            elo = std::make_unique<elo::EloPentanomial>(stats);
        } else {
            elo = std::make_unique<elo::EloWDL>(stats);
        }

        auto timeFirst  = getTime(engines.first);
        auto timeSecond = getTime(engines.second);
        auto tc =
            timeFirst == timeSecond ? fmt::format("{}", timeFirst) : fmt::format("{} - {}", timeFirst, timeSecond);

        auto threadsFirst  = getThreads(engines.first);
        auto threadsSecond = getThreads(engines.second);
        auto threads       = threadsFirst == threadsSecond ? fmt::format("{}", threadsFirst)
                                                           : fmt::format("{} - {}", threadsFirst, threadsSecond);

        auto hashFirst  = getHash(engines.first);
        auto hashSecond = getHash(engines.second);

        auto hash = hashFirst == hashSecond ? fmt::format("{}", hashFirst)
                                            : fmt::format("{} - {}", hashFirst, hashSecond);

        const auto games       = stats.wins + stats.losses + stats.draws;
        const auto points      = stats.wins + 0.5 * stats.draws;
        const auto pointsRatio = points / games * 100;

        const auto pairsRatio =
            static_cast<double>(stats.penta_WW + stats.penta_WD) / (stats.penta_LD + stats.penta_LL);

        auto bookname   = book;
        std::size_t pos = bookname.find_last_of("/\\");

        if (pos != std::string::npos) {
            bookname = bookname.substr(pos + 1);
        }

        // engine, engine2, tc, threads, hash, book
        auto line1 = fmt::format("Results of {} vs {} ({}, {}, {}{}):", first, second, tc, threads, hash,
                                 book.empty() ? "" : fmt::format(", {}", bookname));
        auto line2 = fmt::format("Elo: {}, nElo: {}", elo->getElo(), elo->nElo());
        auto line3 =
            fmt::format("LOS: {}, DrawRatio: {}, PairsRatio: {:.2f}", elo->los(), elo->drawRatio(stats), pairsRatio);
        auto line4 = fmt::format("Games: {}, Wins: {}, Losses: {}, Draws: {}, Points: {:.1f} ({:.2f} %)", games,
                                 stats.wins, stats.losses, stats.draws, points, pointsRatio);
        auto line5 = fmt::format("Ptnml(0-2): [{}, {}, {}, {}, {}]", stats.penta_LL, stats.penta_LD,
                                 stats.penta_WL + stats.penta_DD, stats.penta_WD, stats.penta_WW);
        auto lines = fmt::format("{}\n{}\n{}\n{}\n{}\n", line1, line2, line3, line4, line5);

        std::cout << lines << std::flush;
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isEnabled()) {
            double llr = sprt.getLLR(stats, report_penta_);

            auto fmt = fmt::format("LLR: {:.2f} {} {}\n", llr, sprt.getBounds(), sprt.getElo());

            std::cout << fmt << std::flush;
        }
    }

    void startGame(const pair_config& configs, std::size_t current_game_count, std::size_t max_game_count) override {
        auto fmt = fmt::format("Started game {} of {} ({} vs {})\n", current_game_count, max_game_count,
                               configs.first.name, configs.second.name);

        std::cout << fmt << std::flush;
    }

    void endGame(const pair_config& configs, const Stats& stats, const std::string& annotation,
                 std::size_t id) override {
        auto fmt = fmt::format("Finished game {} ({} vs {}): {} {{{}}}\n", id, configs.first.name, configs.second.name,
                               formatStats(stats), annotation);

        std::cout << fmt << std::flush;
    }

    void endTournament() override { std::cout << "Tournament finished" << std::endl; }

   private:
    std::string getTime(const engine::UciEngine& engine) {
        const auto& limit = engine.getConfig().limit;

        if (limit.tc.time > 0) {
            auto moves     = limit.tc.moves > 0 ? fmt::format("{}/", limit.tc.moves) : "";
            auto time      = limit.tc.time / 1000.0;
            auto increment = limit.tc.increment > 0 ? fmt::format("+{:.2g}", limit.tc.increment / 1000.0) : "";

            return fmt::format("{}{}{}", moves, time, increment);
        } else if (limit.tc.fixed_time > 0) {
            auto time = limit.tc.fixed_time / 1000.0;

            return fmt::format("{}/move", time);
        } else if (limit.plies > 0) {
            return fmt::format("{} plies", limit.plies);
        } else if (limit.nodes > 0) {
            return fmt::format("{} nodes", limit.nodes);
        }

        return "";
    }

    std::string getThreads(const engine::UciEngine& engine) {
        return fmt::format("{}{}", engine.getUciOptionValue("Threads").value_or("NULL"),
                                   engine.getUciOptionValue("Threads").has_value() ? "t" : "");
    }

    std::string getHash(const engine::UciEngine& engine) {
        return fmt::format("{}{}", engine.getUciOptionValue("Hash").value_or("NULL"),
                                   engine.getUciOptionValue("Hash").has_value() ? "MB" : "");
    }

    bool report_penta_;
};

}  // namespace fast_chess
