#include "engine.h"
#include "types.h"

Engine::Engine(std::string command)
{
    initProcess(command);
}

bool Engine::pingEngine()
{
    return isResponsive();
}

EngineConfiguration Engine::getConfig() const
{
    return config;
}
