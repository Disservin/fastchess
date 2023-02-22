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
    Tournament(const CMD::GameManagerOptions &mc);

    void startTournament(std::vector<EngineConfiguration> configs);
    std::array<GameResult, 2> startMatch(std::vector<EngineConfiguration> configs, std::string openingFen);

    void loadConfig(const CMD::GameManagerOptions &mc);

    std::string fetchNextFen() const;

  private:
    CMD::GameManagerOptions matchConfig;

    ThreadPool pool = ThreadPool(1);
};