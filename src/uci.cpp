#include "uci.h"

#include <stdexcept>

UciEngine UciEngine::sendUci()
{
    writeProcess("uci");
    return *this;
}

UciEngine UciEngine::sendSetoption(const std::string &name, const std::string &value)
{
    writeProcess("setoption name " + name + " value " + value);
    return *this;
}

UciEngine UciEngine::sendGo(const std::string &limit)
{
    writeProcess("go " + limit);
    return *this;
}

void UciEngine::startEngine(const std::string &cmd /* add engines options here*/)
{
    setCmd(cmd);

    if (!pingProcess())
    {
        throw std::runtime_error("Something went wrong when pinging the engine.");
    }

    sendUci();

    /*
    TODO: set all the engine options
    sendSetoption("Hash", "16");
    */
}
