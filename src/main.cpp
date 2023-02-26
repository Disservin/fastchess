#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engine.h"
#include "options.h"
#include "tournament.h"
#include "uci_engine.h"

namespace
{
Tournament *tour;
}

#ifdef _WIN64

BOOL WINAPI consoleHandler(DWORD signal)
{

    if (signal == CTRL_C_EVENT)
        tour->printElo();

    return TRUE;
}

#else
void sigintHandler(int param)
{
    tour->printElo();
    exit(param);
}

#endif

int main(int argc, char const *argv[])
{
#ifdef _WIN64
    if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
    {
        std::cout << "\nERROR: Could not set control handler";
        return 1;
    }

#else
    signal(SIGINT, sigintHandler);
#endif

    CMD::Options options = CMD::Options(argc, argv);

    for (auto config : options.getEngineConfigs())
    {
        std::cout << "Name:" << config.name << std::endl;
        std::cout << "Command: " << config.cmd << std::endl;
        std::cout << "TC:" << config.tc.moves << "/" << config.tc.time << "+" << config.tc.increment << std::endl;
        std::cout << "Nodes: " << config.nodes << std::endl;
        std::cout << "Plies: " << config.plies << std::endl;
        for (auto option : config.options)
        {
            std::cout << "Option name: " << option.first;
            std::cout << " value: " << option.second << std::endl;
        }
    }

    tour = new Tournament();

    tour->loadConfig(options.getGameOptions());
    tour->startTournament(options.getEngineConfigs());

    delete tour;

    return 0;
}