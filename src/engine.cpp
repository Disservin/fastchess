#include <cassert>
#include <utility>

#include "engine.h"
#include "types.h"

Engine::Engine(std::string command)
{
    initProcess(command);
}

Engine::~Engine()
{
    stopEngine();
}

void Engine::stopEngine()
{
    killProcess();
}

bool Engine::pingEngine()
{
    return isResponsive();
}