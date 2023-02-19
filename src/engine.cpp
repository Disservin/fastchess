#include <cassert>
#include <utility>

#include "engine.h"
#include "types.h"

Engine::Engine(std::string command) : cmd(std::move(command))
{
    initProcess(cmd);
}

Engine::~Engine()
{
    stopEngine();
}

void Engine::setName(const std::string &name)
{
    this->name = name;
}

void Engine::setArgs(const std::string &args)
{
    this->args = args;
}

void Engine::setOptions(const std::vector<std::pair<std::string, std::string>> &options)
{
    this->options = options;
}

void Engine::setTc(const TimeControl &tc)
{
    this->tc = tc;
}

void Engine::setNodeLimit(const uint64_t nodes)
{
    this->nodes = nodes;
}

void Engine::setPlyLimit(const uint64_t plies)
{
    this->plies = plies;
}

void Engine::setCmd(const std::string &command)
{
    this->cmd = command;
    initProcess(cmd);
}

std::string Engine::getName() const
{
    return name;
}

std::string Engine::getCmd() const
{
    return cmd;
}

std::string Engine::getArgs() const
{
    return args;
}

std::vector<std::pair<std::string, std::string>> Engine::getOptions() const
{
    return options;
}

TimeControl Engine::getTc() const
{
    return tc;
}
uint64_t Engine::getNodeLimit() const
{
    return nodes;
}
uint64_t Engine::getPlyLimit() const
{
    return plies;
}
void Engine::stopEngine()
{
    killProcess();
}

bool Engine::pingEngine()
{
    return isResponsive();
}