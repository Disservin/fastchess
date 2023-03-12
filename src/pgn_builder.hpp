#pragma once

#include "options.hpp"
#include "tournament.hpp"

namespace fast_chess
{

class PgnBuilder
{
  private:
    std::string pgn_;

  public:
    PgnBuilder(const Match &match, const CMD::GameManagerOptions &game_options_);

    std::string getPGN() const;
};

} // namespace fast_chess