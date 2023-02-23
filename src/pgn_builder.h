#pragma once
#include "tournament.h"

#include <vector>

class PgnBuilder
{
  private:
    /* data */
  public:
    PgnBuilder(std::vector<Match> matches);
    ~PgnBuilder();
};