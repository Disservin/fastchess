#include "engine.h"

#include <cassert>

Engine::Engine(const std::string &command) : cmd(command), process(command)
{
}

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

void Engine::setTimeout(int64_t timeout)
{
    this->timeout = timeout;
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

int64_t Engine::getTimeout() const
{
    return timeout;
}

void Engine::startProcess()
{
    bool timedOut = false;
    process.writeEngine("uci");
    auto uciHeader = process.readEngine("uciok", timeout, timedOut);

    assert(!timedOut);
}

void Engine::stopProcess()
{
    process.writeEngine("quit");
}

void Engine::pingProcess()
{
    bool timedOut = false;
    process.writeEngine("isready");
    process.readEngine("readyok", timeout, timedOut);

    assert(!timedOut);
}

void Engine::writeProcess(const std::string &input)
{
    process.writeEngine(input);
}

std::vector<std::string> Engine::readProcess(const std::string &last_word, int64_t tm)
{
    bool timedOut = false;
    return process.readEngine(last_word, timeout, timedOut);
}