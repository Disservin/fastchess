#include <matchmaking/elo/elo_logistic.hpp>

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

EloLogistic::EloLogistic(const Stats& stats) {
    diff_      = getDiff(stats);
    error_     = getError(stats);
    nelodiff_  = getneloDiff(stats);
    neloerror_ = getneloError(stats);
}

double EloLogistic::percToEloDiff(double percentage) noexcept {
    return -400.0 * std::log10(1.0 / percentage - 1.0);
}

double EloLogistic::percToNeloDiff(double percentage, double stdev) noexcept {
    return (percentage - 0.5) / (std::sqrt(2) * stdev) * (800 / std::log(10));
}

double EloLogistic::percToNeloDiffWDL(double percentage, double stdev) noexcept {
    return (percentage - 0.5) / stdev * (800 / std::log(10));
}

double EloLogistic::getError(const Stats& stats) noexcept {
    const double n    = stats.wins + stats.losses + stats.draws;
    const double w    = stats.wins / n;
    const double l    = stats.losses / n;
    const double d    = stats.draws / n;
    const double perc = w + d / 2.0;

    const double devW  = w * std::pow(1.0 - perc, 2.0);
    const double devL  = l * std::pow(0.0 - perc, 2.0);
    const double devD  = d * std::pow(0.5 - perc, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);

    const double devMin = perc - 1.959963984540054 * stdev;
    const double devMax = perc + 1.959963984540054 * stdev;
    return (percToEloDiff(devMax) - percToEloDiff(devMin)) / 2.0;
}

double EloLogistic::getneloError(const Stats& stats) noexcept {
    const double n    = stats.wins + stats.losses + stats.draws;
    const double w    = stats.wins / n;
    const double l    = stats.losses / n;
    const double d    = stats.draws / n;
    const double perc = w + d / 2.0;

    const double devW  = w * std::pow(1.0 - perc, 2.0);
    const double devL  = l * std::pow(0.0 - perc, 2.0);
    const double devD  = d * std::pow(0.5 - perc, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);

    const double devMin = perc - 1.959963984540054 * stdev;
    const double devMax = perc + 1.959963984540054 * stdev;
    return (percToNeloDiffWDL(devMax, stdev * std::sqrt(n)) -
            percToNeloDiffWDL(devMin, stdev * std::sqrt(n))) /
           2.0;
}

double EloLogistic::getDiff(const Stats& stats) noexcept {
    const double n          = stats.wins + stats.losses + stats.draws;
    const double score      = stats.wins + stats.draws / 2.0;
    const double percentage = (score / n);

    return percToEloDiff(percentage);
}

double EloLogistic::getneloDiff(const Stats& stats) noexcept {
    const double n    = stats.wins + stats.losses + stats.draws;
    const double w    = stats.wins / n;
    const double l    = stats.losses / n;
    const double d    = stats.draws / n;
    const double perc = w + d / 2.0;

    const double devW  = w * std::pow(1.0 - perc, 2.0);
    const double devL  = l * std::pow(0.0 - perc, 2.0);
    const double devD  = d * std::pow(0.5 - perc, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);
    return percToNeloDiffWDL(perc, stdev * std::sqrt(n));
}

std::string EloLogistic::getnElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;
    return ss.str();
}

std::string EloLogistic::getLos(const Stats& stats) const noexcept {
    const double games = stats.wins + stats.losses + stats.draws;
    const double W     = double(stats.wins) / games;
    const double D     = double(stats.draws) / games;
    const double L     = double(stats.losses) / games;
    const double a     = W + 0.5 * D;
    const double W_dev = W * std::pow((1 - a), 2);
    const double D_dev = D * std::pow((0.5 - a), 2);
    const double L_dev = L * std::pow((0 - a), 2);
    const double stdev = std::sqrt(W_dev + D_dev + L_dev) / std::sqrt(games);

    const double los = (1 - std::erf(-(a - 0.5) / (std::sqrt(2.0) * stdev))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string EloLogistic::getDrawRatio(const Stats& stats) const noexcept {
    const double n = stats.wins + stats.losses + stats.draws;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (stats.draws / n) * 100.0 << " %";
    return ss.str();
}

std::string EloLogistic::getScoreRatio(const Stats& stats) const noexcept {
    const double n        = stats.wins + stats.losses + stats.draws;
    const auto scoreRatio = double(stats.wins * 2 + stats.draws) / (n * 2);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << scoreRatio;
    return ss.str();
}

}  // namespace fast_chess
