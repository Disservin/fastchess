#pragma once

#include <matchmaking/tournament/roundrobin/roundrobin.hpp>
#include <types/tournament.hpp>

namespace fastchess {

// Manages the tournament, currently wraps round robin but can be extended to support
// different tournament types
class TournamentManager {
   public:
    TournamentManager();

    ~TournamentManager() {
        atomic::stop = true;
        Logger::trace("~TournamentManager()");
    }

    void start(int argc, char const* argv[]);
};

}  // namespace fastchess
