#include "uci_engine.h"
#include "engine_config.h"

#include <stdexcept>

void UciEngine::setConfig(const EngineConfiguration &rhs)
{
    config = rhs;
}

void UciEngine::sendUci()
{
    writeProcess("uci");
}

std::vector<std::string> UciEngine::readUci()
{
    bool timeout = false;
    return readProcess("uciok", timeout);
}

void UciEngine::sendQuit()
{
    writeProcess("quit");
}

void UciEngine::sendSetoption(const std::string &name, const std::string &value)
{
    writeProcess("setoption name " + name + " value " + value);
}

void UciEngine::sendGo(const std::string &limit)
{
    writeProcess("go " + limit);
}

void UciEngine::startEngine(const std::string &cmd /* add engines options here*/)
{
    initProcess(cmd);
    sendUci();
    if (!pingEngine())
    {
        throw std::runtime_error("Something went wrong when pinging the engine.");
    }

    /*
    TODO: set all the engine options
    sendSetoption("Hash", "16");
    */
}

void UciEngine::stopEngine()
{
    sendQuit();
    killProcess();
}