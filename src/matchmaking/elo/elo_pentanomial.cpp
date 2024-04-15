#include <matchmaking/elo/elo_pentanomial.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace fast_chess {

EloPentanomial::EloPentanomial(const Stats& stats) {
    diff_      = diff(stats);
    error_     = error(stats);
    nelodiff_  = nEloDiff(stats);
    neloerror_ = nEloError(stats);
}

double EloPentanomial::scoreToEloDiff(double score) noexcept {
    return -400.0 * std::log10(1.0 / score - 1.0);
}

double EloPentanomial::scoreToNeloDiff(double score, double variance) noexcept {
    return (score - 0.5) / std::sqrt(2 * variance) * (800 / std::log(10));
}

double EloPentanomial::calcScore(const Stats& stats) noexcept {
    const double pairs    = total(stats);
    const double WW       = double(stats.penta_WW) / pairs;
    const double WD       = double(stats.penta_WD) / pairs;
    const double WL       = double(stats.penta_WL) / pairs;
    const double DD       = double(stats.penta_DD) / pairs;
    const double LD       = double(stats.penta_LD) / pairs;
    return WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
}

double EloPentanomial::calcVariance(const Stats& stats) noexcept {
    const double score    = calcScore(stats);
    const double pairs    = total(stats);
    const double WW       = double(stats.penta_WW) / pairs;
    const double WD       = double(stats.penta_WD) / pairs;
    const double WL       = double(stats.penta_WL) / pairs;
    const double DD       = double(stats.penta_DD) / pairs;
    const double LD       = double(stats.penta_LD) / pairs;
    const double LL       = double(stats.penta_LL) / pairs;
    const double WW_dev   = WW * std::pow((1 - score), 2);
    const double WD_dev   = WD * std::pow((0.75 - score), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - score), 2);
    const double LD_dev   = LD * std::pow((0.25 - score), 2);
    const double LL_dev   = LL * std::pow((0 - score), 2);
    const double variance = WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev;
    return variance;
}

double EloPentanomial::variancePerGame(const Stats& stats) noexcept {
    return calcVariance(stats) / total(stats);
}

double EloPentanomial::scoreUpperBound(const Stats& stats) noexcept {
    const double CI95zscore = 1.959963984540054;
    return calcScore(stats) + CI95zscore * std::sqrt(variancePerGame(stats));
}

double EloPentanomial::scoreLowerBound(const Stats& stats) noexcept {
    const double CI95zscore = 1.959963984540054;
    return calcScore(stats) - CI95zscore * std::sqrt(variancePerGame(stats));
}

double EloPentanomial::error(const Stats& stats) noexcept {
    return (scoreToEloDiff(scoreUpperBound(stats)) - scoreToEloDiff(scoreLowerBound(stats))) / 2.0;
}

double EloPentanomial::nEloError(const Stats& stats) noexcept {
    const double variance = calcVariance(stats);
    return (scoreToNeloDiff(scoreUpperBound(stats), std::sqrt(variance)) -
            scoreToNeloDiff(scoreLowerBound(stats), std::sqrt(variance))) /
            2.0;
}

double EloPentanomial::diff(const Stats& stats) noexcept {
    return scoreToEloDiff(calcScore(stats));
}

double EloPentanomial::nEloDiff(const Stats& stats) noexcept {
    return scoreToNeloDiff(calcScore(stats), std::sqrt(calcVariance(stats)));
}

std::string EloPentanomial::nElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;
    return ss.str();
}

std::string EloPentanomial::los(const Stats& stats) const noexcept {
    const double los = (1 - std::erf(-(calcScore(stats) - 0.5) / std::sqrt(2.0 * variancePerGame(stats)))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string EloPentanomial::drawRatio(const Stats& stats) const noexcept {
    const double pairs = total(stats);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ((stats.penta_WL + stats.penta_DD) / pairs) * 100.0
       << " %";
    return ss.str();
}

std::string EloPentanomial::scoreRatio(const Stats& stats) const noexcept {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << calcScore(stats);
    return ss.str();
}

std::size_t EloPentanomial::total(const Stats& stats) noexcept {
    return stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD + stats.penta_LD +
           stats.penta_LL;
}
}  // namespace fast_chess
