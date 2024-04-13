#include <matchmaking/elo/elo_norm.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace fast_chess {

EloNormalized::EloNormalized(const Stats& stats) {
    diff_      = diff(stats);
    error_     = error(stats);
    nelodiff_  = nEloDiff(stats);
    neloerror_ = nEloError(stats);
}

double EloNormalized::percToEloDiff(double percentage) noexcept {
    return -400.0 * std::log10(1.0 / percentage - 1.0);
}

double EloNormalized::percToNeloDiff(double percentage, double stdev) noexcept {
    return (percentage - 0.5) / (std::sqrt(2) * stdev) * (800 / std::log(10));
}

double EloNormalized::percToNeloDiffWDL(double percentage, double stdev) noexcept {
    return (percentage - 0.5) / stdev * (800 / std::log(10));
}

double EloNormalized::error(const Stats& stats) noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    const double WW       = double(stats.penta_WW) / pairs;
    const double WD       = double(stats.penta_WD) / pairs;
    const double WL       = double(stats.penta_WL) / pairs;
    const double DD       = double(stats.penta_DD) / pairs;
    const double LD       = double(stats.penta_LD) / pairs;
    const double LL       = double(stats.penta_LL) / pairs;
    const double a        = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev   = WW * std::pow((1 - a), 2);
    const double WD_dev   = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev   = LD * std::pow((0.25 - a), 2);
    const double LL_dev   = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    const double devMin = a - 1.959963984540054 * stdev;
    const double devMax = a + 1.959963984540054 * stdev;
    return (percToEloDiff(devMax) - percToEloDiff(devMin)) / 2.0;
}

double EloNormalized::nEloError(const Stats& stats) noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    const double WW       = double(stats.penta_WW) / pairs;
    const double WD       = double(stats.penta_WD) / pairs;
    const double WL       = double(stats.penta_WL) / pairs;
    const double DD       = double(stats.penta_DD) / pairs;
    const double LD       = double(stats.penta_LD) / pairs;
    const double LL       = double(stats.penta_LL) / pairs;
    const double a        = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev   = WW * std::pow((1 - a), 2);
    const double WD_dev   = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev   = LD * std::pow((0.25 - a), 2);
    const double LL_dev   = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    const double devMin = a - 1.959963984540054 * stdev;
    const double devMax = a + 1.959963984540054 * stdev;
    return (percToNeloDiff(devMax, stdev * std::sqrt(pairs)) -
            percToNeloDiff(devMin, stdev * std::sqrt(pairs))) /
           2.0;
}

double EloNormalized::diff(const Stats& stats) noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    const double WW         = double(stats.penta_WW) / pairs;
    const double WD         = double(stats.penta_WD) / pairs;
    const double WL         = double(stats.penta_WL) / pairs;
    const double DD         = double(stats.penta_DD) / pairs;
    const double LD         = double(stats.penta_LD) / pairs;
    const double percentage = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;

    return percToEloDiff(percentage);
}

double EloNormalized::nEloDiff(const Stats& stats) noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    const double WW       = double(stats.penta_WW) / pairs;
    const double WD       = double(stats.penta_WD) / pairs;
    const double WL       = double(stats.penta_WL) / pairs;
    const double DD       = double(stats.penta_DD) / pairs;
    const double LD       = double(stats.penta_LD) / pairs;
    const double LL       = double(stats.penta_LL) / pairs;
    const double a        = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev   = WW * std::pow((1 - a), 2);
    const double WD_dev   = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev   = LD * std::pow((0.25 - a), 2);
    const double LL_dev   = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    return percToNeloDiff(a, stdev * std::sqrt(pairs));
}

std::string EloNormalized::nElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;
    return ss.str();
}

std::string EloNormalized::los(const Stats& stats) const noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    const double WW       = double(stats.penta_WW) / pairs;
    const double WD       = double(stats.penta_WD) / pairs;
    const double WL       = double(stats.penta_WL) / pairs;
    const double DD       = double(stats.penta_DD) / pairs;
    const double LD       = double(stats.penta_LD) / pairs;
    const double LL       = double(stats.penta_LL) / pairs;
    const double a        = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev   = WW * std::pow((1 - a), 2);
    const double WD_dev   = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev   = LD * std::pow((0.25 - a), 2);
    const double LL_dev   = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    const double los = (1 - std::erf(-(a - 0.5) / (std::sqrt(2.0) * stdev))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string EloNormalized::drawRatio(const Stats& stats) const noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ((stats.penta_WL + stats.penta_DD) / pairs) * 100.0
       << " %";
    return ss.str();
}

std::string EloNormalized::scoreRatio(const Stats& stats) const noexcept {
    const double pairs = stats.penta_WW + stats.penta_WD + stats.penta_WL + stats.penta_DD +
                         stats.penta_LD + stats.penta_LL;
    const double WW         = double(stats.penta_WW) / pairs;
    const double WD         = double(stats.penta_WD) / pairs;
    const double WL         = double(stats.penta_WL) / pairs;
    const double DD         = double(stats.penta_DD) / pairs;
    const double LD         = double(stats.penta_LD) / pairs;
    const double scoreRatio = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << scoreRatio;
    return ss.str();
}

}  // namespace fast_chess
