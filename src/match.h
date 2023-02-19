#pragma once

#include "engine_config.h"
#include "uci_engine.h"

struct MatchConfig
{
    /*
    TODO
    */
};

class Match
{
  public:
    Match() = default;
    Match(const MatchConfig &mc);

    void startMatch(std::vector<EngineConfiguration> engines /* Match stuff*/);

    void load_config(const MatchConfig &mc);

  private:
    MatchConfig match_config;
};