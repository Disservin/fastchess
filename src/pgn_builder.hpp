#pragma once

#include "options.hpp"

namespace fast_chess
{

class PgnBuilder
{
  public:
    // PgnBuilder(const MatchInfo &match, const CMD::GameManagerOptions &game_options_,
    //            const bool saveTime);

    std::string getPGN() const;

  private:
    std::string pgn_;
};

} // namespace fast_chess