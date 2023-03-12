#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engines/uci_engine.hpp"
#include "options.hpp"
#include "tournament.hpp"

using namespace fast_chess;

namespace
{
Tournament *Tour;
CMD::Options *Options;
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

        Options->saveJson(Tour->getStats());

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
    Tour->stopPool();
    Options->saveJson(Tour->getStats());

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
    Options = new CMD::Options(argc, argv);
    Tour = new Tournament(Options->getGameOptions());

    Tour->setStats(Options->getStats());

    /*
        for (const auto &config : Options->getEngineConfigs())
        {
            std::cout << "Name:" << config.name << std::endl;
            std::cout << "Command: " << config.cmd << std::endl;
            std::cout << "TC:" << config.tc.moves << "/" << config.tc.time << "+" <<
       config.tc.increment
                      << std::endl;
            std::cout << "ST:" << config.tc.fixed_time << std::endl;
            std::cout << "Nodes: " << config.nodes << std::endl;
            std::cout << "Plies: " << config.plies << std::endl;
            for (const auto &option : config.options)
            {
                std::cout << "Option name: " << option.first;
                std::cout << " value: " << option.second << std::endl;
            }
        }

        std::cout << "Concurrency: " << Options->getGameOptions().concurrency << std::endl;
        std::cout << "Draw enabled: " << Options->getGameOptions().draw.enabled << std::endl;
        std::cout << "Draw movecount: " << Options->getGameOptions().draw.move_count << std::endl;
        std::cout << "Draw movenumber: " << Options->getGameOptions().draw.move_number << std::endl;
        std::cout << "Draw score: " << Options->getGameOptions().draw.score << std::endl;
        std::cout << "Eventname: " << Options->getGameOptions().event_name << std::endl;
        std::cout << "Games: " << Options->getGameOptions().games << std::endl;
        std::cout << "Opening File: " << Options->getGameOptions().opening.file << std::endl;
        std::cout << "Opening Format: " << Options->getGameOptions().opening.format << std::endl;
        std::cout << "Opening Order: " << Options->getGameOptions().opening.order << std::endl;
        std::cout << "Opening Plies: " << Options->getGameOptions().opening.plies << std::endl;
        std::cout << "Opening Start: " << Options->getGameOptions().opening.start << std::endl;
        std::cout << "PGN File: " << Options->getGameOptions().pgn.file << std::endl;
        std::cout << "TrackNodes enabled: " << Options->getGameOptions().pgn.track_nodes <<
       std::endl; std::cout << "Ratinginterval: " << Options->getGameOptions().ratinginterval <<
       std::endl; std::cout << "Recover: " << Options->getGameOptions().recover << std::endl;
        std::cout << "Resign enabled: " << Options->getGameOptions().resign.enabled << std::endl;
        std::cout << "Resign Movecount: " << Options->getGameOptions().resign.move_count <<
       std::endl; std::cout << "Resign score: " << Options->getGameOptions().resign.score <<
       std::endl; std::cout << "Rounds: " << Options->getGameOptions().rounds << std::endl;
    */

    Tour->startTournament(Options->getEngineConfigs());

    Options->saveJson(Tour->getStats());

    delete Tour;
    delete Options;

    return 0;
}