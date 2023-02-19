#pragma once

#include "engine.h"

class UciEngine : Engine
{
  public:
    UciEngine() : Engine()
    {
    }

    UciEngine sendUci();
    UciEngine sendSetoption(const std::string &name, const std::string &value);
    UciEngine sendGo(const std::string &limit);

    void startEngine(const std::string &cmd /* cmd and , add engines options here*/);

  private:
};
