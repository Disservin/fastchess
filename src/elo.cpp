#include <cmath>
#include <iomanip>

#include "elo.h"

Elo::Elo(int wins, int draws, int losses)
{
    diff = getDiff(wins, draws, losses);
    error = getError(wins, losses, draws);
}

double Elo::getDiff(double percentage)
{
    return -400.0 * std::log10(1.0 / percentage - 1.0);
}

double Elo::inverseError(double x)
{
    constexpr double pi = 3.1415926535897;

    double a = 8.0 * (pi - 3.0) / (3.0 * pi * (4.0 - pi));
    double y = std::log(1.0 - x * x);
    double z = 2.0 / (pi * a) + y / 2.0;

    double ret = std::sqrt(std::sqrt(z * z - y / a) - z);

    if (x < 0.0)
        return -ret;
    return ret;
}

double Elo::phiInv(double p)
{
    return std::sqrt(2.0) * inverseError(2.0 * p - 1.0);
}

double Elo::getError(int wins, int losses, int draws)
{
    double n = wins + losses + draws;
    double w = wins / n;
    double l = losses / n;
    double d = draws / n;
    double perc = w + d / 2.0;

    double devW = w * std::pow(1.0 - perc, 2.0);
    double devL = l * std::pow(0.0 - perc, 2.0);
    double devD = d * std::pow(0.5 - perc, 2.0);
    double stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);

    double devMin = perc + phiInv(0.025) * stdev;
    double devMax = perc + phiInv(0.975) * stdev;
    return (getDiff(devMax) - getDiff(devMin)) / 2.0;
}

double Elo::getDiff(int wins, int losses, int draws)
{
    double n = wins + losses + draws;
    double score = wins + draws / 2.0;
    double percentage = (score / n);

    return -400.0 * std::log10(1.0 / percentage - 1.0);
}

std::string Elo::getElo() const
{
    std::stringstream ss;

    ss << std::fixed << std::setprecision(2) << diff;
    ss << " +/- ";
    ss << std::fixed << std::setprecision(2) << error;
    return ss.str();
}