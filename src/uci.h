#pragma once

#include "engine.h"

class Uci : Engine
{
  public:
    Uci() : Engine()
    {
    }

    Uci sendUci();
    Uci sendSetoption(const std::string &name, const std::string &value);
    Uci sendGo(const std::string &limit);

    void startEngine(const std::string &cmd /* cmd and , add engines options here*/);

  private:
};
