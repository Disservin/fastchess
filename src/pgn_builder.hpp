#pragma once

#include "options.hpp"
#include "tournament.hpp"

namespace fast_chess
{

class PgnBuilder
{
  public:
    PgnBuilder(const Match &match, const CMD::GameManagerOptions &game_options_);

    std::string getPGN() const;

  private:
    std::string pgn_;
};

} // namespace fast_chess