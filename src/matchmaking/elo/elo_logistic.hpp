#pragma once

#include <string>

#include <matchmaking/elo/elo.hpp>
#include <types/stats.hpp>

namespace fast_chess {

class EloLogistic : public EloBase {
   public:
    EloLogistic(const Stats& stats);

    [[nodiscard]] std::string los(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string drawRatio(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string scoreRatio(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string nElo() const noexcept;

   private:
    [[nodiscard]] static double percToEloDiff(double percentage) noexcept;

    [[nodiscard]] static double percToNeloDiff(double percentage, double stdev) noexcept;

    [[nodiscard]] static double percToNeloDiffWDL(double percentage, double stdev) noexcept;

    [[nodiscard]] static double diff(const Stats& stats) noexcept;

    [[nodiscard]] static double nEloDiff(const Stats& stats) noexcept;

    [[nodiscard]] static double error(const Stats& stats) noexcept;

    [[nodiscard]] static double nEloError(const Stats& stats) noexcept;

    [[nodiscard]] static std::size_t total(const Stats& stats) noexcept;
};

}  // namespace fast_chess
