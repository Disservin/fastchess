#pragma once

#include <string>

namespace fastchess {

class Stats;

enum SPRTResult { SPRT_H0, SPRT_H1, SPRT_CONTINUE };

class SPRT {
   public:
    SPRT() = default;

    SPRT(double alpha, double beta, double elo0, double elo1, std::string model, bool enabled);

    [[nodiscard]] bool isEnabled() const noexcept;

    [[nodiscard]] static double leloToScore(double lelo) noexcept;
    [[nodiscard]] static double bayeseloToScore(double bayeselo, double drawelo) noexcept;
    [[nodiscard]] static double neloToScoreWDL(double nelo, double stdDeviation) noexcept;
    [[nodiscard]] static double neloToScorePenta(double nelo, double stdDeviation) noexcept;
    [[nodiscard]] double getLLR(const Stats& stats, bool penta) const noexcept;
    [[nodiscard]] double getLLR(int win, int draw, int loss) const noexcept;
    [[nodiscard]] double getLLR(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD,
                                int penta_LL) const noexcept;

    [[nodiscard]] SPRTResult getResult(double llr) const noexcept;
    [[nodiscard]] std::string getBounds() const noexcept;
    [[nodiscard]] std::string getElo() const noexcept;
    [[nodiscard]] double getLowerBound() const noexcept;
    [[nodiscard]] double getUpperBound() const noexcept;

   private:
    double lower_ = 0.0;
    double upper_ = 0.0;

    double elo0_ = 0.0;
    double elo1_ = 0.0;

    bool enabled_      = false;
    std::string model_ = "normalized";
};

}  // namespace fastchess
