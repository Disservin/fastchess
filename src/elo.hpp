#include <string>

namespace fast_chess
{
class Elo
{
  private:
    double diff;
    double error;

  public:
    Elo(int wins, int losses, int draws);

    double inverseError(double x);

    double phiInv(double p);

    double getDiff(double percentage);

    double getDiff(int wins, int losses, int draws);

    double getError(int wins, int losses, int draws);

    std::string getElo() const;
};

} // namespace fast_chess
