#pragma once

#include "options.h"
#include "tournament.h"
#include <vector>

class PgnBuilder
{
  private:
    /* data */
  public:
    PgnBuilder::PgnBuilder(const std::vector<Match> &matches, CMD::Options options);
};