#pragma once

#include "engine.h"
#include "types.h"

class UciEngine : public Engine
{
  public:
    UciEngine() : Engine()
    {
    }

    UciEngine(EngineConfiguration config)
    {
        setConfig(config);
    }

    void sendUciNewGame();

    void sendUci();
    std::vector<std::string> readUci();

    std::string buildGoInput();

    void loadConfig(const EngineConfiguration &config);

    void sendQuit();
    void sendSetoption(const std::string &name, const std::string &value);
    void sendGo(const std::string &limit);

    void setConfig(const EngineConfiguration &rhs);

    void startEngine();
    void stopEngine();

    Color color;
};
