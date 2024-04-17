#include <matchmaking/elo/elo_pentanomial.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace fast_chess {

EloPentanomial::EloPentanomial(const Stats& stats) {
    pairs_             = total(stats);
    score_             = calcScore(stats);
    variance_          = calcVariance(stats);
    variance_per_pair_ = variance_ / pairs_;
    CI95zscore_        = 1.959963984540054;
    scoreUpperBound_   = score_ + CI95zscore_ * std::sqrt(variance_per_pair_);
    scoreLowerBound_   = score_ - CI95zscore_ * std::sqrt(variance_per_pair_);
    diff_              = scoreToEloDiff(score_);
    error_             = (scoreToEloDiff(scoreUpperBound_) - scoreToEloDiff(scoreLowerBound_)) / 2.0;
    nelodiff_          = scoreToNeloDiff(score_, variance_);
    neloerror_         = (scoreToNeloDiff(scoreUpperBound_, variance_) 
                         - scoreToNeloDiff(scoreLowerBound_, variance_)) / 2.0;
}

double EloPentanomial::scoreToNeloDiff(double score, double variance) noexcept {
    return (score - 0.5) / std::sqrt(2 * variance) * (800 / std::log(10));
}

double EloPentanomial::calcScore(const Stats& stats) noexcept {
    const double WW    = double(stats.penta_WW) / pairs_;
    const double WD    = double(stats.penta_WD) / pairs_;
    const double WL    = double(stats.penta_WL) / pairs_;
    const double DD    = double(stats.penta_DD) / pairs_;
    const double LD    = double(stats.penta_LD) / pairs_;
    return WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
}

double EloPentanomial::calcVariance(const Stats& stats) noexcept {
    const double WW       = double(stats.penta_WW) / pairs_;
    const double WD       = double(stats.penta_WD) / pairs_;
    const double WL       = double(stats.penta_WL) / pairs_;
    const double DD       = double(stats.penta_DD) / pairs_;
    const double LD       = double(stats.penta_LD) / pairs_;
    const double LL       = double(stats.penta_LL) / pairs_;
    const double WW_dev   = WW * std::pow((1 - score_), 2);
    const double WD_dev   = WD * std::pow((0.75 - score_), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - score_), 2);
    const double LD_dev   = LD * std::pow((0.25 - score_), 2);
    const double LL_dev   = LL * std::pow((0 - score_), 2);
    return WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev;
}

std::string EloPentanomial::nElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;
    return ss.str();
}

std::string EloPentanomial::los() const noexcept {
    const double los =
        (1 - std::erf(-(score_ - 0.5) / std::sqrt(2.0 * variance_per_pair_))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string EloPentanomial::drawRatio(const Stats& stats) const noexcept {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ((stats.penta_WL + stats.penta_DD) / pairs_) * 100.0
       << " %";
    return ss.str();
}

std::string EloPentanomial::printScore() const noexcept {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << score_;
    return ss.str();
}

std::size_t EloPentanomial::total(const Stats& stats) noexcept {
    return stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD + stats.penta_LD +
           stats.penta_LL;
}
}  // namespace fast_chess
