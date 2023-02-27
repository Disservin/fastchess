#include "engine.h"
#include "types.h"

Engine::Engine(std::string command)
{
    initProcess(command);
}

EngineConfiguration Engine::getConfig() const
{
    return config;
}
