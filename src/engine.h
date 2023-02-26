#pragma once

#include "engine_config.h"
#include "engineprocess.h"

class Engine : public EngineProcess
{
  protected:
    EngineConfiguration config;

  public:
    Engine() = default;

    Engine(std::string command);

    bool pingEngine();

    EngineConfiguration getConfig() const;
};
