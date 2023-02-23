#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engine.h"
#include "options.h"
#include "tournament.h"
#include "uci_engine.h"

int main(int argc, char const *argv[])
{
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

    Tournament tour(options.getGameOptions());

    tour.startTournament(options.getEngineConfigs());

    return 0;
}