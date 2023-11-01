#pragma once

#include <string>

namespace fast_chess {
class Elo {
   public:
    Elo(int wins, int losses, int draws);

    [[nodiscard]] static double inverseError(double x) noexcept;

    [[nodiscard]] static double phiInv(double p) noexcept;

    [[nodiscard]] static double percToEloDiff(double percentage) noexcept;

    [[nodiscard]] static double getDiff(int wins, int losses, int draws) noexcept;

    [[nodiscard]] static double getError(int wins, int losses, int draws) noexcept;

    [[nodiscard]] std::string getElo() const noexcept;

    [[nodiscard]] static std::string getLos(int wins, int losses) noexcept;

    [[nodiscard]] static std::string getDrawRatio(int wins, int losses, int draws) noexcept;

   private:
    double diff_;
    double error_;
};

}  // namespace fast_chess
