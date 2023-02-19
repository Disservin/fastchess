#pragma once

#include "engine.h"

class UciEngine : protected Engine
{
  public:
    UciEngine() : Engine()
    {
    }

    UciEngine(Engine engine)
    {
        setEngine(engine);
    }

    UciEngine sendUci();
    UciEngine sendSetoption(const std::string &name, const std::string &value);
    UciEngine sendGo(const std::string &limit);

    UciEngine setEngine(const Engine &rhs);

    void startEngine(const std::string &cmd /* cmd and , add engines options here*/);

  private:
};
