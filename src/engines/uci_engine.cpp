#include "uci_engine.hpp"
#include "engine_config.hpp"

#include <sstream>
#include <stdexcept>

void UciEngine::setConfig(const EngineConfiguration &rhs)
{
    config = rhs;
}

std::string UciEngine::checkErrors(int id)
{
    if (!getError().empty())
    {
        std::stringstream ss;
        ss << getError() << "\nCant write to engine " << getConfig().name
           << (id != -1 ? ("# " + std::to_string(id)) : "");

        if (!getConfig().recover)
        {
            throw std::runtime_error(ss.str());
        }
        return ss.str();
    }
    return "";
}

bool UciEngine::isResponsive(int64_t threshold)
{
    if (!isAlive())
        return false;

    bool timeout = false;
    writeProcess("isready");
    readProcess("readyok", timeout, threshold);
    return !timeout;
}

void UciEngine::sendUciNewGame()
{
    writeProcess("ucinewgame");
    isResponsive(60000);
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

std::string UciEngine::buildGoInput(Color stm, const TimeControl &tc, const TimeControl &tc_2) const
{
    std::stringstream input;
    input << "go";

    if (config.nodes != 0)
        input << " nodes " << config.nodes;

    if (config.plies != 0)
        input << " depth " << config.plies;

    if (tc.time != 0)
    {
        if (stm == WHITE)
        {
            input << " wtime " << tc.time << " btime " << tc_2.time;
        }
        else
        {
            input << " wtime " << tc_2.time << " btime " << tc.time;
        }
    }

    if (tc.increment != 0)
    {
        if (stm == WHITE)
        {
            input << " winc " << tc.increment << " binc " << tc_2.increment;
        }
        else
        {
            input << " winc " << tc_2.increment << " binc " << tc.increment;
        }
    }

    if (tc.moves != 0)
        input << " movestogo " << tc.moves;

    return input.str();
}

void UciEngine::loadConfig(const EngineConfiguration &config)
{
    this->config = config;
}

void UciEngine::sendQuit()
{
    writeProcess("quit");
    checkErrors();
}

void UciEngine::sendSetoption(const std::string &name, const std::string &value)
{
    writeProcess("setoption name " + name + " value " + value);
}

void UciEngine::sendGo(const std::string &limit)
{
    writeProcess("go " + limit);
}

void UciEngine::restartEngine()
{
    resetError();
    killProcess();
    initProcess(config.cmd);
}

void UciEngine::startEngine()
{
    initProcess(config.cmd);

    sendUci();
    readUci();

    if (!isResponsive(60000))
    {
        throw std::runtime_error("Something went wrong when pinging the engine.");
    }

    for (const auto &option : config.options)
    {
        sendSetoption(option.first, option.second);
    }
}

void UciEngine::startEngine(const std::string &cmd)
{
    initProcess(cmd);

    sendUci();
    readUci();

    if (!isResponsive(60000))
    {
        throw std::runtime_error("Something went wrong when pinging the engine.");
    }

    for (const auto &option : config.options)
    {
        sendSetoption(option.first, option.second);
    }
}
