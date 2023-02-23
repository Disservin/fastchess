#pragma once

#include "options.h"
#include "tournament.h"
#include <vector>

class PgnBuilder
{
  private:
    /* data */
  public:
    PgnBuilder(const std::vector<Match> &matches, CMD::GameManagerOptions gameOptions);
};