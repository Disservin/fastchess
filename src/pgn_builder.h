#pragma once

#include "options.h"
#include "tournament.h"

class PgnBuilder
{
  private:
    std::string pgn;

  public:
    PgnBuilder(const Match &match, const CMD::GameManagerOptions &gameOptions);

    std::string getPGN() const;
};
