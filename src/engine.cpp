#include "engine.h"

#include <cassert>

Engine::Engine(const std::string &command) : cmd(command), process(command) {}

void Engine::setName(const std::string &name)
{
    this->name = name;
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

void Engine::startProcess()
{
    bool timedOut = false;
    process.writeEngine("uci");
    auto uciHeader = process.readEngine("uciok", 1000, timedOut);

    assert(!timedOut);
}

void Engine::stopProcess()
{
    process.writeEngine("quit");
}

void Engine::pingProcess() {
    bool timedOut = false;
    process.writeEngine("isready");
    process.readEngine("readyok", 1000, timedOut);

    assert(!timedOut);
}