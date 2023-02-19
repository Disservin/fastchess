#pragma once

#include "uci_engine.h"

class Match
{
  public:
    Match();

    Match(Match &&) = default;

    Match(const Match &) = default;

    Match &operator=(Match &&) = default;

    Match &operator=(const Match &) = default;

    ~Match();

    void startMatch(std::vector<Engine> engines /* Match stuff*/);

  private:
};

Match::Match()
{
}

Match::~Match()
{
}