#pragma once

#include "board.h"
#include "engine_config.h"
#include "threadpool.h"
#include "uci_engine.h"

#include <array>

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

    void startTournament(std::vector<EngineConfiguration> configs);
    std::array<GameResult, 2> startMatch(std::vector<EngineConfiguration> configs);

    void loadConfig(const Tournament &mc);

  private:
    TournamentConfig match_config;

    ThreadPool pool = ThreadPool(1);
};