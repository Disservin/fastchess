#pragma once

#include <string>

#include <matchmaking/elo/elo.hpp>
#include <types/stats.hpp>

namespace fast_chess {

class EloPentanomial : public EloBase {
   public:
    EloPentanomial(const Stats& stats);

    [[nodiscard]] std::string los(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string drawRatio(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string printScore(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string nElo() const noexcept override;

   private:
    [[nodiscard]] static double scoreToEloDiff(double scoreentage) noexcept;

    [[nodiscard]] static double scoreToNeloDiff(double scoreentage, double stdev) noexcept;

    [[nodiscard]] static double score(const Stats& stats) noexcept;

    [[nodiscard]] static double variance(const Stats& stats) noexcept;

    [[nodiscard]] static double variancePerGame(const Stats& stats) noexcept;

    [[nodiscard]] static double scoreUpperBound(const Stats& stats) noexcept;

    [[nodiscard]] static double scoreLowerBound(const Stats& stats) noexcept;

    [[nodiscard]] static double diff(const Stats& stats) noexcept;

    [[nodiscard]] static double nEloDiff(const Stats& stats) noexcept;

    [[nodiscard]] static double error(const Stats& stats) noexcept;

    [[nodiscard]] static double nEloError(const Stats& stats) noexcept;

    [[nodiscard]] static std::size_t total(const Stats& stats) noexcept;
};

}  // namespace fast_chess
