#include <sprt.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

#include <logger.hpp>

namespace fast_chess {

SPRT::SPRT(double alpha, double beta, double elo0, double elo1) {
    this->valid_ = alpha != 0.0 && beta != 0.0 && elo0 < elo1;
    if (isValid()) {
        this->lower_ = std::log(beta / (1 - alpha));
        this->upper_ = std::log((1 - beta) / alpha);
        this->s0_ = getLL(elo0);
        this->s1_ = getLL(elo1);

        this->elo0_ = elo0;
        this->elo1_ = elo1;

        Logger::cout("Initialized valid SPRT configuration.");
    } else if (!(alpha == 0.0 && beta == 0.0 && elo0 == 0.0 && elo1 == 0.0)) {
        Logger::cout("No valid SPRT configuration was found!");
    }
}

double SPRT::getLL(double elo) { return 1.0 / (1.0 + std::pow(10.0, -elo / 400.0)); }

double SPRT::getLLR(int win, int draw, int loss) const {
    if (win == 0 || draw == 0 || loss == 0 || !valid_) return 0.0;

    const double games = win + draw + loss;
    const double W = double(win) / games, D = double(draw) / games;
    const double a = W + D / 2;
    const double b = W + D / 4;
    const double var = b - std::pow(a, 2);
    const double var_s = var / games;
    return (s1_ - s0_) * (2 * a - s0_ - s1_) / var_s / 2.0;
}

SPRTResult SPRT::getResult(double llr) const {
    if (!valid_) return SPRT_CONTINUE;

    if (llr > upper_)
        return SPRT_H0;
    else if (llr < lower_)
        return SPRT_H1;
    else
        return SPRT_CONTINUE;
}

std::string SPRT::getBounds() const {
    std::stringstream ss;
    ss << "(" << std::fixed << std::setprecision(2) << lower_ << ", " << std::fixed
       << std::setprecision(2) << upper_ << ")";
    return ss.str();
}

std::string SPRT::getElo() const {
    std::stringstream ss;
    ss << "[" << std::fixed << std::setprecision(2) << elo0_ << ", " << std::fixed
       << std::setprecision(2) << elo1_ << "]";

    return ss.str();
}

bool SPRT::isValid() const { return valid_; }

}  // namespace fast_chess
