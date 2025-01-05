#pragma once

#include <string>

#include <matchmaking/stats.hpp>

namespace fastchess::elo {

class EloBase {
   public:
    EloBase() = default;

    virtual ~EloBase() = default;

    [[nodiscard]] std::string getElo() const noexcept;
    [[nodiscard]] virtual std::string los() const noexcept  = 0;
    [[nodiscard]] virtual std::string nElo() const noexcept = 0;

    [[nodiscard]] virtual double getScore() const noexcept = 0;

    [[nodiscard]] double scoreToEloDiff(double score) const noexcept { return -400.0 * std::log10(1.0 / score - 1.0); }

    [[nodiscard]] double nEloDiff() const noexcept { return nelodiff_; }
    [[nodiscard]] double nEloError() const noexcept { return neloerror_; }

    [[nodiscard]] double diff() const noexcept { return diff_; }
    [[nodiscard]] double error() const noexcept { return error_; }

   protected:
    static constexpr double CI95ZSCORE = 1.959963984540054;

    double games_;
    double pairs_;
    double score_;
    double variance_;
    double variance_per_game_;
    double variance_per_pair_;
    double scoreUpperBound_;
    double scoreLowerBound_;
    double diff_;
    double error_;
    double nelodiff_;
    double neloerror_;
};

}  // namespace fastchess::elo
