#include <string>

namespace fast_chess
{
class Elo
{
  public:
    Elo(int wins, int losses, int draws);

    double inverseError(double x) const;

    double phiInv(double p) const;

    double getDiff(double percentage) const;

    double getDiff(int wins, int losses, int draws) const;

    double getError(int wins, int losses, int draws) const;

    std::string getElo() const;

  private:
    double diff_;
    double error_;
};

} // namespace fast_chess
