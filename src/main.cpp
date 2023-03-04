#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engines/engine.hpp"
#include "engines/uci_engine.hpp"
#include "options.hpp"
#include "tournament.hpp"

namespace
{
Tournament *Tour;
} // namespace

#ifdef _WIN64

BOOL WINAPI consoleHandler(DWORD signal)
{
    switch (signal)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_C_EVENT:

        Tour->printElo();
        Tour->stopPool();

        return TRUE;
    default:
        break;
    }

    return FALSE;
}

#else
void sigintHandler(int param)
{
    Tour->printElo();
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

    Tour = new Tournament(options.getGameOptions());

    for (const auto &config : options.getEngineConfigs())
    {
        std::cout << "Name:" << config.name << std::endl;
        std::cout << "Command: " << config.cmd << std::endl;
        std::cout << "TC:" << config.tc.moves << "/" << config.tc.time << "+" << config.tc.increment
                  << std::endl;
        std::cout << "TS:" << config.tc.fixed_time << std::endl;
        std::cout << "Nodes: " << config.nodes << std::endl;
        std::cout << "Plies: " << config.plies << std::endl;
        for (const auto &option : config.options)
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
    std::cout << "Resign enabled: " << options.getGameOptions().resign.enabled << std::endl;
    std::cout << "Resign Movecount: " << options.getGameOptions().resign.moveCount << std::endl;
    std::cout << "Resign score: " << options.getGameOptions().resign.score << std::endl;
    std::cout << "Rounds: " << options.getGameOptions().rounds << std::endl;

    Tour->loadConfig(options.getGameOptions());
    Tour->startTournament(options.getEngineConfigs());

    delete Tour;

    return 0;
}
