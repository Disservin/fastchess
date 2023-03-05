#include <string>

enum SPRTResult
{
    SPRT_H0,
    SPRT_H1,
    SPRT_CONTINUE
};

class SPRT
{
  private:
    double lower = 0.0;
    double upper = 0.0;
    double s0 = 0.0;
    double s1 = 0.0;

    bool valid = false;

    double elo0 = 0;
    double elo1 = 0;

  public:
    SPRT(double alpha, double beta, double elo0, double elo1);

    SPRT() = default;

    static double getLL(double elo);

    double getLLR(int win, int draw, int loss) const;

    SPRTResult getResult(double llr) const;

    std::string getBounds() const;

    std::string getElo() const;

    bool isValid() const;
};
