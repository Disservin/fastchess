#include <matchmaking/elo/elo.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace fast_chess {

Elo::Elo(int wins, int losses, int draws) {
    diff_  = getDiff(wins, losses, draws);
    error_ = getError(wins, losses, draws);
    nelodiff_  = getneloDiff(wins, losses, draws);
    neloerror_ = getneloError(wins, losses, draws);
}

Elo::Elo(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) {
    diff_  = getDiff(penta_WW, penta_WD, penta_WL, penta_DD, penta_LD, penta_LL);
    error_ = getError(penta_WW, penta_WD, penta_WL, penta_DD, penta_LD, penta_LL);
    nelodiff_  = getneloDiff(penta_WW, penta_WD, penta_WL, penta_DD, penta_LD, penta_LL);
    neloerror_ = getneloError(penta_WW, penta_WD, penta_WL, penta_DD, penta_LD, penta_LL);
}

double Elo::percToEloDiff(double percentage) noexcept {
    return -400.0 * std::log10(1.0 / percentage - 1.0);
}

double Elo::percToNeloDiff(double percentage, double stdev) noexcept {
    return (percentage - 0.5) / (std::sqrt(2) * stdev) * (800 / std::log(10));
}

double Elo::percToNeloDiffWDL(double percentage, double stdev) noexcept {
    return (percentage - 0.5) / stdev * (800 / std::log(10));
}

double Elo::getError(int wins, int losses, int draws) noexcept {
    const double n    = wins + losses + draws;
    const double w    = wins / n;
    const double l    = losses / n;
    const double d    = draws / n;
    const double perc = w + d / 2.0;

    const double devW  = w * std::pow(1.0 - perc, 2.0);
    const double devL  = l * std::pow(0.0 - perc, 2.0);
    const double devD  = d * std::pow(0.5 - perc, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);

    const double devMin = perc - 1.959963984540054 * stdev;
    const double devMax = perc + 1.959963984540054 * stdev;
    return (percToEloDiff(devMax) - percToEloDiff(devMin)) / 2.0;
}

double Elo::getneloError(int wins, int losses, int draws) noexcept {
    const double n    = wins + losses + draws;
    const double w    = wins / n;
    const double l    = losses / n;
    const double d    = draws / n;
    const double perc = w + d / 2.0;

    const double devW  = w * std::pow(1.0 - perc, 2.0);
    const double devL  = l * std::pow(0.0 - perc, 2.0);
    const double devD  = d * std::pow(0.5 - perc, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);

    const double devMin = perc - 1.959963984540054 * stdev;
    const double devMax = perc + 1.959963984540054 * stdev;
    return (percToNeloDiffWDL(devMax, stdev * std::sqrt(n)) - percToNeloDiffWDL(devMin, stdev * std::sqrt(n))) / 2.0;
}

double Elo::getError(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs    = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    const double WW = double(penta_WW) / pairs; 
    const double WD = double(penta_WD) / pairs; 
    const double WL = double(penta_WL) / pairs; 
    const double DD = double(penta_DD) / pairs;
    const double LD = double(penta_LD) / pairs;
    const double LL = double(penta_LL) / pairs;
    const double a = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev = WW * std::pow((1 - a), 2);
    const double WD_dev = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev = LD * std::pow((0.25 - a), 2);
    const double LL_dev = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    const double devMin = a - 1.959963984540054 * stdev;
    const double devMax = a + 1.959963984540054 * stdev;
    return (percToEloDiff(devMax) - percToEloDiff(devMin)) / 2.0;
}

double Elo::getneloError(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs    = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    const double WW = double(penta_WW) / pairs; 
    const double WD = double(penta_WD) / pairs; 
    const double WL = double(penta_WL) / pairs; 
    const double DD = double(penta_DD) / pairs;
    const double LD = double(penta_LD) / pairs;
    const double LL = double(penta_LL) / pairs;
    const double a = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev = WW * std::pow((1 - a), 2);
    const double WD_dev = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev = LD * std::pow((0.25 - a), 2);
    const double LL_dev = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    const double devMin = a - 1.959963984540054 * stdev;
    const double devMax = a + 1.959963984540054 * stdev;
    return (percToNeloDiff(devMax, stdev * std::sqrt(pairs)) - percToNeloDiff(devMin, stdev * std::sqrt(pairs))) / 2.0;
}

double Elo::getDiff(int wins, int losses, int draws) noexcept {
    const double n          = wins + losses + draws;
    const double score      = wins + draws / 2.0;
    const double percentage = (score / n);

    return percToEloDiff(percentage);
}

double Elo::getneloDiff(int wins, int losses, int draws) noexcept {
    const double n    = wins + losses + draws;
    const double w    = wins / n;
    const double l    = losses / n;
    const double d    = draws / n;
    const double perc = w + d / 2.0;

    const double devW  = w * std::pow(1.0 - perc, 2.0);
    const double devL  = l * std::pow(0.0 - perc, 2.0);
    const double devD  = d * std::pow(0.5 - perc, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);
    return percToNeloDiffWDL(perc, stdev * std::sqrt(n));
}

double Elo::getDiff(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs          = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    const double WW = double(penta_WW) / pairs; 
    const double WD = double(penta_WD) / pairs; 
    const double WL = double(penta_WL) / pairs; 
    const double DD = double(penta_DD) / pairs;
    const double LD = double(penta_LD) / pairs;
    const double percentage = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;

    return percToEloDiff(percentage);
}

double Elo::getneloDiff(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs    = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    const double WW = double(penta_WW) / pairs; 
    const double WD = double(penta_WD) / pairs; 
    const double WL = double(penta_WL) / pairs; 
    const double DD = double(penta_DD) / pairs;
    const double LD = double(penta_LD) / pairs;
    const double LL = double(penta_LL) / pairs;
    const double a = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev = WW * std::pow((1 - a), 2);
    const double WD_dev = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev = LD * std::pow((0.25 - a), 2);
    const double LL_dev = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    return percToNeloDiff(a, stdev * std::sqrt(pairs));
}

std::string Elo::getElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << diff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << error_;
    return ss.str();
}

std::string Elo::getnElo() const noexcept {
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << nelodiff_;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << neloerror_;
    return ss.str();
}

std::string Elo::getLos(int wins, int losses, int draws) noexcept {
    const double games    = wins + losses + draws;
    const double W = double(wins) / games; 
    const double D = double(draws) / games; 
    const double L = double(losses) / games; 
    const double a = W + 0.5 * D;
    const double W_dev = W * std::pow((1 - a), 2);
    const double D_dev = D * std::pow((0.5 - a), 2);
    const double L_dev = L * std::pow((0 - a), 2);
    const double stdev = std::sqrt(W_dev + D_dev + L_dev) / std::sqrt(games);
    
    const double los = (1 - std::erf(-(a-0.5) / (std::sqrt(2.0) * stdev))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string Elo::getLos(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs    = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    const double WW = double(penta_WW) / pairs; 
    const double WD = double(penta_WD) / pairs; 
    const double WL = double(penta_WL) / pairs; 
    const double DD = double(penta_DD) / pairs;
    const double LD = double(penta_LD) / pairs;
    const double LL = double(penta_LL) / pairs;
    const double a = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev = WW * std::pow((1 - a), 2);
    const double WD_dev = WD * std::pow((0.75 - a), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - a), 2);
    const double LD_dev = LD * std::pow((0.25 - a), 2);
    const double LL_dev = LL * std::pow((0 - a), 2);
    const double stdev = std::sqrt(WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev) / std::sqrt(pairs);

    const double los = (1 - std::erf(-(a-0.5) / (std::sqrt(2.0) * stdev))) / 2.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << los * 100.0 << " %";
    return ss.str();
}

std::string Elo::getDrawRatio(int wins, int losses, int draws) noexcept {
    const double n = wins + losses + draws;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (draws / n) * 100.0 << " %";
    return ss.str();
}

std::string Elo::getDrawRatio(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs    = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ((penta_WL + penta_DD) / pairs) * 100.0 << " %";
    return ss.str();
}

std::string Elo::getScoreRatio(int wins, int losses, int draws) noexcept {
    const double n        = wins + losses + draws;
    const auto scoreRatio = double(wins * 2 + draws) / (n * 2);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << scoreRatio;
    return ss.str();
}

std::string Elo::getScoreRatio(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept {
    const double pairs    = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    const double WW = double(penta_WW) / pairs; 
    const double WD = double(penta_WD) / pairs; 
    const double WL = double(penta_WL) / pairs; 
    const double DD = double(penta_DD) / pairs;
    const double LD = double(penta_LD) / pairs;
    const double scoreRatio = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << scoreRatio;
    return ss.str();
}

}  // namespace fast_chess
