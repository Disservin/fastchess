#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine_config.h"
#include "engineprocess.h"

class Engine : public EngineProcess
{
  protected:
    EngineConfiguration config;

  public:
    Engine() = default;

    Engine(std::string command);

    ~Engine();

    void stopEngine();

    bool pingEngine();

    EngineConfiguration getConfig() const;
};
