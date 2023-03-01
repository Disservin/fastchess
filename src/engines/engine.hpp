#pragma once

#include "engine_config.hpp"
#include "engineprocess.hpp"

class Engine : public EngineProcess
{
  protected:
    EngineConfiguration config;

  public:
    Engine() = default;

    Engine(const std::string &command);

    EngineConfiguration getConfig() const;
};
