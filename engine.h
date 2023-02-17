#pragma once

#include <cstdint>
#include <string>

struct TimeControl
{
    uint64_t moves;
    uint64_t time;
    uint64_t increment;
};

class Engine
{
  private:
    std::string name;
    std::string cmd;
    std::string arg;
    TimeControl tc;

  public:
    Engine(/* args */);
    ~Engine();
};

Engine::Engine(/* args */)
{
}

Engine::~Engine()
{
}
