#pragma once

#include <cli/cli_args.hpp>
#include <types/tournament.hpp>

namespace fastchess {

// Manages the tournament, currently wraps round robin but can be extended to support
// different tournament types
class TournamentManager {
   public:
    TournamentManager();
    ~TournamentManager();

    void start(const cli::Args& args);
};

}  // namespace fastchess
