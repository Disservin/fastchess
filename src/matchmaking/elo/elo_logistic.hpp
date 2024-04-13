#pragma once

#include <string>

#include <matchmaking/elo/elo.hpp>
#include <types/stats.hpp>

namespace fast_chess {

class EloLogistic : public EloBase {
   public:
    EloLogistic(const Stats& stats);

    [[nodiscard]] std::string getLos(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string getDrawRatio(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string getScoreRatio(const Stats& stats) const noexcept override;
    [[nodiscard]] std::string getnElo() const noexcept;

   private:
    [[nodiscard]] static double percToEloDiff(double percentage) noexcept;

    [[nodiscard]] static double percToNeloDiff(double percentage, double stdev) noexcept;

    [[nodiscard]] static double percToNeloDiffWDL(double percentage, double stdev) noexcept;

    [[nodiscard]] static double getDiff(const Stats& stats) noexcept;

    [[nodiscard]] static double getneloDiff(const Stats& stats) noexcept;

    [[nodiscard]] static double getError(const Stats& stats) noexcept;

    [[nodiscard]] static double getneloError(const Stats& stats) noexcept;
};

}  // namespace fast_chess
