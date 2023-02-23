#pragma once

#include <array>
#include <vector>

#include "board.h"
#include "engine_config.h"
#include "options.h"
#include "threadpool.h"
#include "uci_engine.h"

struct Match
{
    std::time_t matchStartTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::vector<Move> moves;
    GameResult result;
    EngineConfiguration whiteEngine;
    EngineConfiguration blackEngine;
    int round;
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