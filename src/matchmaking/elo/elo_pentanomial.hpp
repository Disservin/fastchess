#pragma once

#include <string>

#include <matchmaking/elo/elo.hpp>
#include <types/stats.hpp>

namespace fast_chess {

class EloPentanomial : public EloBase {
   public:
    EloPentanomial(const Stats& stats);

    [[nodiscard]] std::string los() const noexcept override;
    [[nodiscard]] std::string drawRatio(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string printScore() const noexcept override;
    [[nodiscard]] std::string nElo() const noexcept override;

   private:
    [[nodiscard]] static double scoreToNeloDiff(double score, double variance) noexcept;

    [[nodiscard]] double calcScore(const Stats& stats) noexcept;

    [[nodiscard]] double calcVariance(const Stats& stats) noexcept;

    [[nodiscard]] static std::size_t total(const Stats& stats) noexcept;
};

}  // namespace fast_chess
