#pragma once

class Match
{
  public:
    Match();
    Match(Match &&) = default;
    Match(const Match &) = default;
    Match &operator=(Match &&) = default;
    Match &operator=(const Match &) = default;
    ~Match();

  private:
};

Match::Match()
{
}

Match::~Match()
{
}