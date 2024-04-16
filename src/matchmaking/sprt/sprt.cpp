#include <matchmaking/sprt/sprt.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

#include <util/logger/logger.hpp>

namespace fast_chess {

SPRT::SPRT(double alpha, double beta, double elo0, double elo1, bool logistic_bounds) {
    valid_ = alpha != 0.0 && beta != 0.0 && elo0 < elo1;
    if (isValid()) {
        lower_ = std::log(beta / (1 - alpha));
        upper_ = std::log((1 - beta) / alpha);

        elo0_ = elo0;
        elo1_ = elo1;

        logistic_bounds_ = logistic_bounds;

        Logger::log<Logger::Level::INFO>("Initialized valid SPRT configuration.");
    } else if (!(alpha == 0.0 && beta == 0.0 && elo0 == 0.0 && elo1 == 0.0)) {
        Logger::log<Logger::Level::INFO>("No valid SPRT configuration was found!");
    }
}

double SPRT::leloToScore(double lelo) noexcept { return 1 / (1 + std::pow(10, (-lelo / 400))); }

double SPRT::neloToScoreWDL(double nelo, double variance) noexcept {
    return nelo * std::sqrt(variance) / (800.0 / std::log(10)) + 0.5;
}

double SPRT::neloToScorePenta(double nelo, double variance) noexcept {
    return nelo * std::sqrt(2.0 * variance) / (800.0 / std::log(10)) + 0.5;
}

double SPRT::getLLR(int win, int draw, int loss) const noexcept {
    if (!valid_) return 0.0;

    const double games = win + draw + loss;
    if (games == 0) return 0.0;
    const double W        = double(win) / games;
    const double D        = double(draw) / games;
    const double L        = double(loss) / games;
    const double score    = W + 0.5 * D;
    const double W_dev    = W * std::pow((1 - score), 2);
    const double D_dev    = D * std::pow((0.5 - score), 2);
    const double L_dev    = L * std::pow((0 - score), 2);
    const double variance = W_dev + D_dev + L_dev;
    if (variance == 0) return 0.0;
    const double variance_per_game = variance / games;
    double score0;
    double score1;
    if (logistic_bounds_ == false) {
        score0 = neloToScoreWDL(elo0_, variance);
        score1 = neloToScoreWDL(elo1_, variance);
    } else {
        score0 = leloToScore(elo0_);
        score1 = leloToScore(elo1_);
    }
    return (score1 - score0) * (2 * score - score0 - score1) / (2 * variance_per_game);
}

double SPRT::getLLR(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD,
                    int penta_LL) const noexcept {
    if (!valid_) return 0.0;

    const double pairs = penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL;
    if (pairs == 0) return 0.0;
    const double WW       = double(penta_WW) / pairs;
    const double WD       = double(penta_WD) / pairs;
    const double WL       = double(penta_WL) / pairs;
    const double DD       = double(penta_DD) / pairs;
    const double LD       = double(penta_LD) / pairs;
    const double LL       = double(penta_LL) / pairs;
    const double score    = WW + 0.75 * WD + 0.5 * (WL + DD) + 0.25 * LD;
    const double WW_dev   = WW * std::pow((1 - score), 2);
    const double WD_dev   = WD * std::pow((0.75 - score), 2);
    const double WLDD_dev = (WL + DD) * std::pow((0.5 - score), 2);
    const double LD_dev   = LD * std::pow((0.25 - score), 2);
    const double LL_dev   = LL * std::pow((0 - score), 2);
    const double variance = WW_dev + WD_dev + WLDD_dev + LD_dev + LL_dev;
    if (variance == 0) return 0.0;
    const double variance_per_pair = variance / pairs;
    double score0;
    double score1;
    if (logistic_bounds_ == false) {
        score0 = neloToScorePenta(elo0_, variance);
        score1 = neloToScorePenta(elo1_, variance);
    } else {
        score0 = leloToScore(elo0_);
        score1 = leloToScore(elo1_);
    }
    return (score1 - score0) * (2 * score - score0 - score1) / (2 * variance_per_pair);
}

SPRTResult SPRT::getResult(double llr) const noexcept {
    if (!valid_) return SPRT_CONTINUE;

    if (llr >= upper_)
        return SPRT_H0;
    else if (llr <= lower_)
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
