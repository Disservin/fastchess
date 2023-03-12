#pragma once

#include <string>

namespace fast_chess
{

enum SPRTResult
{
    SPRT_H0,
    SPRT_H1,
    SPRT_CONTINUE
};

class SPRT
{
  public:
    SPRT() = default;

    SPRT(double alpha, double beta, double elo0, double elo1);

    bool isValid() const;

    static double getLL(double elo);
    double getLLR(int win, int draw, int loss) const;

    SPRTResult getResult(double llr) const;
    std::string getBounds() const;
    std::string getElo() const;

  private:
    double lower_ = 0.0;
    double upper_ = 0.0;
    double s0_ = 0.0;
    double s1_ = 0.0;

    double elo0_ = 0;
    double elo1_ = 0;

    bool valid_ = false;
};

} // namespace fast_chess
