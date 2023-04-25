#include <string>

namespace fast_chess {
class Elo {
   public:
    Elo(int wins, int losses, int draws);

    [[nodiscard]] static double inverseError(double x);

    [[nodiscard]] static double phiInv(double p);

    [[nodiscard]] static double getDiff(double percentage);

    [[nodiscard]] static double getDiff(int wins, int losses, int draws);

    [[nodiscard]] double getError(int wins, int losses, int draws) const;

    [[nodiscard]] std::string getElo() const;

    [[nodiscard]] static std::string getLos(int wins, int losses);

    [[nodiscard]] static std::string getDrawRatio(int wins, int losses, int draws);

   private:
    double diff_;
    double error_;
};

}  // namespace fast_chess
