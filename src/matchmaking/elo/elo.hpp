#pragma once

#include <string>

namespace fast_chess {
class Elo {
   public:
    Elo(int wins, int losses, int draws);

    Elo(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL);

    [[nodiscard]] static double percToEloDiff(double percentage) noexcept;

    [[nodiscard]] static double getDiff(int wins, int losses, int draws) noexcept;

    [[nodiscard]] static double getDiff(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept;

    [[nodiscard]] static double getError(int wins, int losses, int draws) noexcept;

    [[nodiscard]] static double getError(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept;

    [[nodiscard]] std::string getElo() const noexcept;

    [[nodiscard]] static std::string getLos(int wins, int losses, int draws) noexcept;

    [[nodiscard]] static std::string getLos(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept;

    [[nodiscard]] static std::string getDrawRatio(int wins, int losses, int draws) noexcept;

    [[nodiscard]] static std::string getDrawRatio(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept;

    [[nodiscard]] static std::string getScoreRatio(int wins, int losses, int draws) noexcept;

    [[nodiscard]] static std::string getScoreRatio(int penta_WW, int penta_WD, int penta_WL, int penta_DD, int penta_LD, int penta_LL) noexcept;

   private:
    double diff_;
    double error_;
};

}  // namespace fast_chess
