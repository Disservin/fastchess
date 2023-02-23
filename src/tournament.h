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
    std::vector<Move> moves;
    GameResult result;
    EngineConfiguration whiteEngine;
    EngineConfiguration blackEngine;
    std::string startTime;
    std::string endTime;
    std::string duration;
    std::string date;
    Board board;
    int round;
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

    std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");
    std::string formatDuration(std::chrono::seconds duration);
};