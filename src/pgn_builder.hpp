#pragma once

#include "options.hpp"
#include "tournament.hpp"

namespace fast_chess
{

class PgnBuilder
{
  private:
    std::string pgn;

  public:
    PgnBuilder(const Match &match, const CMD::GameManagerOptions &gameOptions);

    std::string getPGN() const;
};

} // namespace fast_chess