#include <string>

namespace fast_chess {
class Elo {
   public:
    Elo(int wins, int losses, int draws);

    static double inverseError(double x);

    static double phiInv(double p);

    static double getDiff(double percentage);

    static double getDiff(int wins, int losses, int draws);

    double getError(int wins, int losses, int draws) const;

    std::string getElo() const;

   private:
    double diff_;
    double error_;
};

}  // namespace fast_chess
