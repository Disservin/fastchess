#pragma once

#include <elo/elo_wdl.hpp>
#include <matchmaking/output/output.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

class Cutechess : public IOutput {
   public:
    void printInterval(const SPRT& sprt, const Stats& stats, const std::string& first, const std::string& second,
                       const engines& engines, const std::string& book) override {
        printElo(stats, first, second, engines, book);
        printSprt(sprt, stats);
    };

    void printResult(const Stats& stats, const std::string& first, const std::string& second) override {
        const elo::EloWDL elo(stats);

        auto fmt = fmt::format("Score of {} vs {}: {} - {} - {} [{}] {}\n", first, second, stats.wins, stats.losses,
                               stats.draws, elo.printScore(), stats.wins + stats.losses + stats.draws);

        std::cout << fmt << std::flush;
    }

    void printElo(const Stats& stats, const std::string&, const std::string&, const engines&,
                  const std::string&) override {
        const elo::EloWDL elo(stats);

        auto fmt =
            fmt::format("Elo difference: {}, LOS: {}, DrawRatio: {}\n", elo.getElo(), elo.los(), elo.drawRatio(stats));

        std::cout << fmt << std::flush;
    }

    void printSprt(const SPRT& sprt, const Stats& stats) override {
        if (sprt.isEnabled()) {
            auto fmt = fmt::format("LLR: {:.2f} {} {}\n", sprt.getLLR(stats.wins, stats.draws, stats.losses),
                                   sprt.getBounds(), sprt.getElo());

            std::cout << fmt << std::flush;
        }
    };

    void startGame(const pair_config& configs, std::size_t current_game_count, std::size_t max_game_count) override {
        auto fmt = fmt::format("Started game {} of {} ({} vs {})\n", current_game_count, max_game_count,
                               configs.first.name, configs.second.name);

        std::cout << fmt << std::flush;
    }

    void endGame(const pair_config& configs, const Stats& stats, const std::string& annotation,
                 std::size_t id) override {
        auto fmt = fmt::format("Finished game {} ({} vs {}): {} {{}}\n", id, configs.first.name, configs.second.name,
                               formatStats(stats), annotation);

        std::cout << fmt << std::flush;
    }

    void endTournament() override { std::cout << "Tournament finished" << std::endl; }
};

}  // namespace fast_chess
