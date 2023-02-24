class Elo
{
  private:
    double mu;
    double stdev;

  public:
    Elo(int wins, int draws, int losses);

    double diff() const;

    double diff(double p) const;
};
