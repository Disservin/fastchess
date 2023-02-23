#pragma once

#include "board.h"
#include "engine_config.h"
#include "options.h"
#include "threadpool.h"
#include "uci_engine.h"

#include <array>
#include <vector>

struct Match
{
    std::vector<Move> moves;
    GameResult result;
    Board board;
};

class Tournament
{
  public:
    Tournament() = default;
    Tournament(const CMD::GameManagerOptions &mc);

    void startTournament(std::vector<EngineConfiguration> configs);
    std::vector<Match> startMatch(std::vector<EngineConfiguration> configs, std::string openingFen);

    void loadConfig(const CMD::GameManagerOptions &mc);

    std::string fetchNextFen() const;

  private:
    CMD::GameManagerOptions matchConfig;

    ThreadPool pool = ThreadPool(1);
};