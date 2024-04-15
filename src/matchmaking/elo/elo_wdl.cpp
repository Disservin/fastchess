#include <matchmaking/elo/elo_wdl.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace fast_chess {

std::string EloBase::getElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << diff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << error_;
    return ss.str();
}

EloWDL::EloWDL(const Stats& stats) {
    diff_      = diff(stats);
    error_     = error(stats);
    nelodiff_  = nEloDiff(stats);
    neloerror_ = nEloError(stats);
}

double EloWDL::scoreToEloDiff(double score) noexcept {
    return -400.0 * std::log10(1.0 / score - 1.0);
}

double EloWDL::scoreToNeloDiff(double score, double variance) noexcept {
    return (score - 0.5) / std::sqrt(variance) * (800 / std::log(10));
}

double EloWDL::calcScore(const Stats& stats) noexcept {
    const double games    = total(stats);
    const double W       = double(stats.wins) / games;
    const double D       = double(stats.draws) / games;
    return W + 0.5 * D;
}

double EloWDL::calcVariance(const Stats& stats) noexcept {
    const double score    = calcScore(stats);
    const double games    = total(stats);
    const double W       = double(stats.wins) / games;
    const double D       = double(stats.draws) / games;
    const double L       = double(stats.losses) / games;
    const double W_dev   = W * std::pow((1 - score), 2);
    const double D_dev   = D * std::pow((0.5 - score), 2);
    const double L_dev   = L * std::pow((0 - score), 2);
    const double variance = W_dev + D_dev + L_dev;
    return variance;
}

double EloWDL::variancePerGame(const Stats& stats) noexcept {
    return calcVariance(stats) / total(stats);
}

double EloWDL::scoreUpperBound(const Stats& stats) noexcept {
    const double CI95zscore = 1.959963984540054;
    return calcScore(stats) + CI95zscore * std::sqrt(variancePerGame(stats));
}

double EloWDL::scoreLowerBound(const Stats& stats) noexcept {
    const double CI95zscore = 1.959963984540054;
    return calcScore(stats) - CI95zscore * std::sqrt(variancePerGame(stats));
}

double EloWDL::error(const Stats& stats) noexcept {
    return (scoreToEloDiff(scoreUpperBound(stats)) - scoreToEloDiff(scoreLowerBound(stats))) / 2.0;
}

double EloWDL::nEloError(const Stats& stats) noexcept {
    const double variance = calcVariance(stats);
    return (scoreToNeloDiff(scoreUpperBound(stats), variance) -
            scoreToNeloDiff(scoreLowerBound(stats), variance)) /
            2.0;
}

double EloWDL::diff(const Stats& stats) noexcept {
    return scoreToEloDiff(calcScore(stats));
}

double EloWDL::nEloDiff(const Stats& stats) noexcept {
    return scoreToNeloDiff(calcScore(stats), calcVariance(stats));
}

std::string EloWDL::nElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;
    return ss.str();
}

std::string EloWDL::los(const Stats& stats) const noexcept {
    const double los = (1 - std::erf(-(calcScore(stats) - 0.5) / std::sqrt(2.0 * variancePerGame(stats)))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string EloWDL::drawRatio(const Stats& stats) const noexcept {
    const double games = total(stats);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ((stats.draws) / games) * 100.0
       << " %";
    return ss.str();
}

std::string EloWDL::scoreRatio(const Stats& stats) const noexcept {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << calcScore(stats);
    return ss.str();
}

std::size_t EloWDL::total(const Stats& stats) noexcept {
    return stats.wins + stats.draws + stats.losses;
}
}  // namespace fast_chess
