#pragma once

#include "board.h"
#include "engine_config.h"
#include "options.h"
#include "threadpool.h"
#include "uci_engine.h"

#include <array>

class Tournament
{
  public:
    Tournament() = default;
    Tournament(const CMD::TournamentConfig &mc);

    void startTournament(std::vector<EngineConfiguration> configs);
    std::array<GameResult, 2> startMatch(std::vector<EngineConfiguration> configs);

    void loadConfig(const CMD::TournamentConfig &mc);

  private:
    CMD::TournamentConfig match_config;

    ThreadPool pool = ThreadPool(1);
};