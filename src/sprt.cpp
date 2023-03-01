#include "sprt.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

SPRT::SPRT(double alpha, double beta, double elo0, double elo1)
{
    this->valid = alpha != 0.0 && beta != 0.0 && elo0 < elo1;
    if (isValid())
    {
        this->lower = std::log(beta / (1 - alpha));
        this->upper = std::log((1 - beta) / alpha);
        this->s0 = getLL(elo0);
        this->s1 = getLL(elo1);

        std::cout << "Initialized valid SPRT configuration." << std::endl;
    }
    else
    {
        std::cout << "No valid SPRT configuration was found!" << std::endl;
    }
}

double SPRT::getLL(double elo)
{
    return 1.0 / (1.0 + std::pow(10.0, -elo / 400.0));
}

double SPRT::getLLR(int win, int draw, int loss) const
{
    if (win == 0 || draw == 0 || loss == 0 || !valid)
        return 0.0;

    double games = win + draw + loss;
    double W = double(win) / games, D = double(draw) / games;
    double a = W + D / 2;
    double b = W + D / 4;
    double var = b - std::pow(a, 2);
    double var_s = var / games;
    return (s1 - s0) * (2 * a - s0 - s1) / var_s / 2.0;
}

SPRTResult SPRT::getResult(double llr) const
{
    if (!valid)
        return SPRT_CONTINUE;

    if (llr > upper)
        return SPRT_H0;
    else if (llr < lower)
        return SPRT_H1;
    else
        return SPRT_CONTINUE;
}

std::string SPRT::getBounds() const
{
    std::stringstream ss;
    ss << "[" << std::fixed << std::setprecision(2) << lower << " ; " << std::fixed << std::setprecision(2) << upper
       << "]";
    return ss.str();
}

bool SPRT::isValid() const
{
    return valid;
}
