#include <cmath>

#include "elo.h"

Elo::Elo(int wins, int draws, int losses)
{
    double n = wins + losses + draws;
    double w = wins / n;
    double l = losses / n;
    double d = draws / n;
    mu = w + d / 2.0;

    double devW = w * std::pow(1.0 - mu, 2.0);
    double devL = l * std::pow(0.0 - mu, 2.0);
    double devD = d * std::pow(0.5 - mu, 2.0);
    stdev = std::sqrt(devW + devL + devD) / std::sqrt(n);
}

double Elo::diff(double p) const
{
	return -400.0 * std::log10(1.0 / p - 1.0);
}

double Elo::diff() const
{
	return diff(mu);
}

