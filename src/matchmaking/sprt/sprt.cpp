#include <matchmaking/sprt/sprt.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

#include <util/logger/logger.hpp>

namespace fast_chess {

SPRT::SPRT(double alpha, double beta, double elo0, double elo1) {
    valid_ = alpha != 0.0 && beta != 0.0 && elo0 < elo1;
    if (isValid()) {
        lower_ = std::log(beta / (1 - alpha));
        upper_ = std::log((1 - beta) / alpha);
        s0_    = getLL(elo0);
        s1_    = getLL(elo1);

        elo0_ = elo0;
        elo1_ = elo1;

        Logger::log<Logger::Level::INFO>("Initialized valid SPRT configuration.");
    } else if (!(alpha == 0.0 && beta == 0.0 && elo0 == 0.0 && elo1 == 0.0)) {
        Logger::log<Logger::Level::INFO>("No valid SPRT configuration was found!");
    }
}

double SPRT::getLL(double elo) noexcept { return 1.0 / (1.0 + std::pow(10.0, -elo / 400.0)); }

double SPRT::getLLR(int win, int draw, int loss) const noexcept {
    if (!valid_) return 0.0;

    const double games = win + draw + loss;
    if (games == 0) return 0.0;
    const double W = double(win) / games, D = double(draw) / games;
    const double a     = W + D / 2;
    const double b     = W + D / 4;
    const double var   = b - std::pow(a, 2);
    if (var == 0) return 0.0;
    const double var_s = var / games;
    return (s1_ - s0_) * (2 * a - s0_ - s1_) / var_s / 2.0;
}

double SPRT::getPentaLLR(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) const noexcept {
    if (!valid_) return 0.0;

    const double pairs = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    if (pairs == 0) return 0.0;
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
    const double var_penta   = WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev;
    if (var_penta == 0) return 0.0;
    const double var_s_penta = var_penta / pairs;
    return (s1_ - s0_) * (2 * a - s0_ - s1_) / var_s_penta / 2.0;
}

SPRTResult SPRT::getResult(double llr) const noexcept {
    if (!valid_) return SPRT_CONTINUE;

    if (llr > upper_)
        return SPRT_H0;
    else if (llr < lower_)
        return SPRT_H1;
    else
        return SPRT_CONTINUE;
}

std::string SPRT::getBounds() const noexcept {
    std::stringstream ss;
    ss << "(" << std::fixed << std::setprecision(2) << lower_ << ", " << std::fixed
       << std::setprecision(2) << upper_ << ")";
    return ss.str();
}

std::string SPRT::getElo() const noexcept {
    std::stringstream ss;
    ss << "[" << std::fixed << std::setprecision(2) << elo0_ << ", " << std::fixed
       << std::setprecision(2) << elo1_ << "]";

    return ss.str();
}

bool SPRT::isValid() const noexcept { return valid_; }

}  // namespace fast_chess
