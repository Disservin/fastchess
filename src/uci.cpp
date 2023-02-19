#include "uci.h"

Uci Uci::sendUci()
{
    writeProcess("uci");
    return *this;
}

Uci Uci::sendSetoption(const std::string &name, const std::string &value)
{
    writeProcess("setoption name " + name + " value " + value);
    return *this;
}

Uci Uci::sendGo(const std::string &limit)
{
    writeProcess("go " + limit);
    return *this;
}

void Uci::startEngine(const std::string &cmd /* add engines options here*/)
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
