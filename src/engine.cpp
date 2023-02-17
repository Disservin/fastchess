#include "engine.h"

void Engine::setName(const std::string &name)
{
    this->name = name;
}

void Engine::setCmd(const std::string &cmd)
{
    this->cmd = cmd;
}

void Engine::setArgs(const std::string &args)
{
    this->args = args;
}

void Engine::setOptions(const std::vector<std::string> &options)
{
    this->options = options;
}

void Engine::setTc(const TimeControl &tc)
{
    this->tc = tc;
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

std::vector<std::string> Engine::getOptions() const
{
    return options;
}

TimeControl Engine::getTc() const
{
    return tc;
}