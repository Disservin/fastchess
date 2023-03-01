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
    switch (signal)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_C_EVENT:

        tour->printElo();
        tour->stopPool();

        return TRUE;
    default:
        break;
    }

    return FALSE;
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
        std::cout << "\nERROR: Could not set control handler\n";
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

    std::cout << "Concurrency: " << options.getGameOptions().concurrency << std::endl;
    std::cout << "Draw enabled: " << options.getGameOptions().draw.enabled << std::endl;
    std::cout << "Draw movecount: " << options.getGameOptions().draw.moveCount << std::endl;
    std::cout << "Draw movenumber: " << options.getGameOptions().draw.moveNumber << std::endl;
    std::cout << "Draw score: " << options.getGameOptions().draw.score << std::endl;
    std::cout << "Eventname: " << options.getGameOptions().eventName << std::endl;
    std::cout << "Games: " << options.getGameOptions().games << std::endl;
    std::cout << "Opening File: " << options.getGameOptions().opening.file << std::endl;
    std::cout << "Opening Format: " << options.getGameOptions().opening.format << std::endl;
    std::cout << "Opening Order: " << options.getGameOptions().opening.order << std::endl;
    std::cout << "Opening Plies: " << options.getGameOptions().opening.plies << std::endl;
    std::cout << "Opening Start: " << options.getGameOptions().opening.start << std::endl;
    std::cout << "PGN File: " << options.getGameOptions().pgn.file << std::endl;
    std::cout << "TrackNodes enabled: " << options.getGameOptions().pgn.trackNodes << std::endl;
    std::cout << "Ratinginterval: " << options.getGameOptions().ratinginterval << std::endl;
    std::cout << "Recover: " << options.getGameOptions().recover << std::endl;
    std::cout << "Repeat: " << options.getGameOptions().repeat << std::endl;
    std::cout << "Resign enabled: " << options.getGameOptions().resign.enabled << std::endl;
    std::cout << "Resign Movecount: " << options.getGameOptions().resign.moveCount << std::endl;
    std::cout << "Resign score: " << options.getGameOptions().resign.score << std::endl;
    std::cout << "Rounds: " << options.getGameOptions().rounds << std::endl;

    tour = new Tournament();

    tour->loadConfig(options.getGameOptions());
    tour->startTournament(options.getEngineConfigs());

    delete tour;

    return 0;
}