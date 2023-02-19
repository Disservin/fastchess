#include <cassert>

#include "engine.h"
#include "types.h"

Engine::Engine(const std::string &command) : cmd(command)
{
    process.initProcess(cmd);
}

Engine::~Engine()
{
    stopProcess();
}

Engine Engine::setName(const std::string &name)
{
    this->name = name;
    return *this;
}

Engine Engine::setArgs(const std::string &args)
{
    this->args = args;
    return *this;
}

Engine Engine::setOptions(const std::vector<std::string> &options)
{
    this->options = options;
    return *this;
}

Engine Engine::setTc(const TimeControl &tc)
{
    this->tc = tc;
    return *this;
}

Engine Engine::setCmd(const std::string &command)
{
    this->cmd = command;
    process.initProcess(cmd);
    return *this;
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
    bool timedOut;
    process.writeEngine("uci");
    auto uciHeader = process.readEngine("uciok", PING_TIMEOUT_THRESHOLD, timedOut);

    assert(!timedOut);

    pingProcess();
}

void Engine::stopProcess()
{
    process.writeEngine("quit");
}

void Engine::pingProcess()
{
    bool timedOut;
    process.writeEngine("isready");
    process.readEngine("readyok", PING_TIMEOUT_THRESHOLD, timedOut);

    assert(!timedOut);
}

void Engine::writeProcess(const std::string &input)
{
    process.writeEngine(input);
}

std::vector<std::string> Engine::readProcess(const std::string &last_word, int64_t timeoutThreshold)
{
    bool timedOut;
    return process.readEngine(last_word, timeoutThreshold, timedOut);
}

std::vector<std::string> Engine::readProcess(const std::string &last_word, bool &timedOut, int64_t timeoutThreshold)
{
    return process.readEngine(last_word, timeoutThreshold, timedOut);
}