#include "engine.hpp"

Engine::Engine(const std::string &command)
{
    initProcess(command);
}

EngineConfiguration Engine::getConfig() const
{
    return config;
}
