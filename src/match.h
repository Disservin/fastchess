#pragma once

#include "engine_config.h"
#include "threadpool.h"
#include "uci_engine.h"

struct TournamentConfig
{
    /*
    TODO
    */
};

class Tournament
{
  public:
    Tournament() = default;
    Tournament(const TournamentConfig &mc);

    void startTournament(/* Tournament stuff*/);
    std::vector<std::string> startMatch(int i /* Tournament stuff*/);

    void loadConfig(const Tournament &mc);

  private:
    TournamentConfig match_config;

    ThreadPool pool = ThreadPool(1);
};