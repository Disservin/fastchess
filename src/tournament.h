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
    Tournament(bool saveTime) : saveTimeHeader(saveTime){};
    Tournament(const CMD::GameManagerOptions &mc);

    void loadConfig(const CMD::GameManagerOptions &mc);
    void startTournament(std::vector<EngineConfiguration> configs);

    Match startMatch(UciEngine &engine1, UciEngine &engine2, int round, std::string openingFen);
    std::vector<Match> runH2H(CMD::GameManagerOptions localMatchConfig, std::vector<EngineConfiguration> configs,
                              int gameId);

    std::string fetchNextFen() const;

    std::vector<std::string> getPGNS() const;

  private:
    ThreadPool pool = ThreadPool(1);
    CMD::GameManagerOptions matchConfig;
    std::vector<std::string> pgns;
    bool saveTimeHeader = true;

    std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");
    std::string formatDuration(std::chrono::seconds duration);
};