#pragma once

#include <string>

#include <elo/elo.hpp>
#include <matchmaking/stats.hpp>

namespace fastchess::elo {

class EloWDL : public EloBase {
   public:
    EloWDL(const Stats& stats);

    [[nodiscard]] std::string los() const noexcept override;
    [[nodiscard]] std::string printScore() const noexcept override;
    [[nodiscard]] std::string nElo() const noexcept override;

   private:
    [[nodiscard]] static double scoreToNeloDiff(double score, double variance) noexcept;

    [[nodiscard]] double calcScore(const Stats& stats) noexcept;

    [[nodiscard]] double calcVariance(const Stats& stats) noexcept;

    [[nodiscard]] static std::size_t total(const Stats& stats) noexcept;
};

}  // namespace fastchess::elo
