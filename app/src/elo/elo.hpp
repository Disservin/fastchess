#pragma once

#include <string>

#include <matchmaking/stats.hpp>

namespace fastchess::elo {

class EloBase {
   public:
    EloBase() = default;

    virtual ~EloBase() = default;

    [[nodiscard]] std::string getElo() const noexcept;
    [[nodiscard]] virtual std::string los() const noexcept                         = 0;
    [[nodiscard]] virtual std::string drawRatio(const Stats& stats) const noexcept = 0;
    [[nodiscard]] virtual std::string printScore() const noexcept                  = 0;
    [[nodiscard]] virtual std::string nElo() const noexcept                        = 0;

    double scoreToEloDiff(double score) noexcept { return -400.0 * std::log10(1.0 / score - 1.0); }

    double nEloDiff() const noexcept { return nelodiff_; }
    double nEloError() const noexcept { return neloerror_; }

    double diff() const noexcept { return diff_; }
    double error() const noexcept { return error_; }

   protected:
    double games_;
    double pairs_;
    double score_;
    double variance_;
    double variance_per_game_;
    double variance_per_pair_;
    double CI95zscore_;
    double scoreUpperBound_;
    double scoreLowerBound_;
    double diff_;
    double error_;
    double nelodiff_;
    double neloerror_;
};

}  // namespace fastchess::elo
