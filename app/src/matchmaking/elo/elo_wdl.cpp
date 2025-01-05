#include <matchmaking/elo/elo_wdl.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

#define FMT_HEADER_ONLY
#include "../../../third_party/fmt/include/fmt/core.h"
#include "../../../third_party/fmt/include/fmt/std.h"

namespace fastchess::elo {

std::string EloBase::getElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << diff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << error_;

    return ss.str();
}

EloWDL::EloWDL(const Stats& stats) {
    games_             = stats.sum();
    score_             = calcScore(stats);
    variance_          = calcVariance(stats);
    variance_per_game_ = variance_ / games_;
    scoreUpperBound_   = score_ + CI95ZSCORE * std::sqrt(variance_per_game_);
    scoreLowerBound_   = score_ - CI95ZSCORE * std::sqrt(variance_per_game_);
    diff_              = scoreToEloDiff(score_);
    error_             = (scoreToEloDiff(scoreUpperBound_) - scoreToEloDiff(scoreLowerBound_)) / 2.0;
    nelodiff_          = scoreToNeloDiff(score_, variance_);
    neloerror_ = (scoreToNeloDiff(scoreUpperBound_, variance_) - scoreToNeloDiff(scoreLowerBound_, variance_)) / 2.0;
}

double EloWDL::scoreToNeloDiff(double score, double variance) noexcept {
    return (score - 0.5) / std::sqrt(variance) * (800 / std::log(10));
}

double EloWDL::calcScore(const Stats& stats) const noexcept {
    const double W = double(stats.wins) / games_;
    const double D = double(stats.draws) / games_;

    return W + 0.5 * D;
}

double EloWDL::calcVariance(const Stats& stats) const noexcept {
    const double W     = double(stats.wins) / games_;
    const double D     = double(stats.draws) / games_;
    const double L     = double(stats.losses) / games_;
    const double W_dev = W * std::pow((1 - score_), 2);
    const double D_dev = D * std::pow((0.5 - score_), 2);
    const double L_dev = L * std::pow((0 - score_), 2);

    return W_dev + D_dev + L_dev;
}

std::string EloWDL::nElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;

    return ss.str();
}

std::string EloWDL::los() const noexcept {
    const double los = (1 - std::erf(-(score_ - 0.5) / std::sqrt(2.0 * variance_per_game_))) / 2.0;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";

    return ss.str();
}

double EloWDL::getScore() const noexcept { return score_; }
}  // namespace fastchess::elo
