#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <elo/elo_wdl.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fastchess {

class Cutechess : public IOutput {
   public:
    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first, const std::string& second,
                       const engines& engines, const std::string& book, ScoreBoard& scoreboard) override {
        std::cout                                                         //
            << printElo(stats, first, second, engines, book, scoreboard)  //
            << printSprt(sprt, stats)                                     //
            << std::flush;
    };

    void printResult(const Stats& stats, const std::string& first, const std::string& second) override {
        const elo::EloWDL elo(stats);

        std::cout << fmt::format("Score of {} vs {}: {} - {} - {}  [{}] {}\n", first, second, stats.wins, stats.losses,
                                 stats.draws, elo.printScore(), stats.wins + stats.losses + stats.draws)
                  << std::flush;
    }

    std::string printElo(const Stats& stats, const std::string&, const std::string&, const engines&, const std::string&,
                         ScoreBoard& scoreboard) override {
        const auto& ecs = config::EngineConfigs.get();

        if (ecs.size() == 2) {
            const elo::EloWDL elo(stats);

            return fmt::format("Elo difference: {}, LOS: {}, DrawRatio: {:.2f}%\n", elo.getElo(), elo.los(),
                               stats.drawRatio());
        }

        std::vector<std::tuple<const EngineConfiguration*, elo::EloWDL, Stats>> elos;

        for (auto& e : ecs) {
            const auto stats = scoreboard.getAllStats(e.name);
            elos.emplace_back(&e, elo::EloWDL(stats), stats);
        }

        // sort by elo diff

        std::sort(elos.begin(), elos.end(),
                  [](const auto& a, const auto& b) { return std::get<1>(a).diff() > std::get<1>(b).diff(); });

        int rank        = 0;
        std::string out = fmt::format("{:<4} {:<25} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}\n", "Rank", "Name",
                                      "Elo", "+/-", "nElo", "+/-", "Games", "Score", "Draw");

        for (const auto& [ec, elo, stats] : elos) {
            out += fmt::format("{:>4} {:<25} {:>10.2f} {:>10.2f} {:>10.2f} {:>10.2f} {:>10} {:>9.1f}% {:>9.1f}%\n",
                               ++rank, ec->name, elo.diff(), elo.error(), elo.nEloDiff(), elo.nEloError(), stats.sum(),
                               stats.pointsRatio(), stats.drawRatio());
        }

        return out;
    }

    std::string printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isEnabled()) {
            double lowerBound = sprt.getLowerBound();
            double upperBound = sprt.getUpperBound();
            double llr        = sprt.getLLR(stats, false);
            double percentage = llr < 0 ? llr / lowerBound * 100 : llr / upperBound * 100;

            std::string result;
            if (llr >= upperBound) {
                result = " - H1 was accepted";
            } else if (llr < lowerBound) {
                result = " - H0 was accepted";
            }

            return fmt::format("SPRT: llr {:.2f} ({:.1f}%), lbound {:.2f}, ubound {:.2f}{}\n", llr, percentage,
                               lowerBound, upperBound, result);
        }

        return "";
    };

    void startGame(const GamePair<EngineConfiguration, EngineConfiguration>& configs, std::size_t current_game_count,
                   std::size_t max_game_count) override {
        std::cout << fmt::format("Started game {} of {} ({} vs {})\n", current_game_count, max_game_count,
                                 configs.white.name, configs.black.name)
                  << std::flush;
    }

    void endGame(const GamePair<EngineConfiguration, EngineConfiguration>& configs, const Stats& stats,
                 const std::string& annotation, std::size_t id) override {
        std::cout << fmt::format("Finished game {} ({} vs {}): {} {{{}}}\n", id, configs.white.name, configs.black.name,
                                 formatStats(stats), annotation)
                  << std::flush;
    }

    void endTournament() override { std::cout << "Tournament finished" << std::endl; }

    OutputType getType() const override { return OutputType::CUTECHESS; }
};

}  // namespace fastchess
