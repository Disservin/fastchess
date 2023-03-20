#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engines/uci_engine.hpp"
#include "matchmaking/tournament.hpp"
#include "options.hpp"

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

        // Tour->printElo();
        Tour->stop();

        Options->saveJson(Tour->getResults());

        return TRUE;
    default:
        break;
    }

    return FALSE;
}

#else
void sigintHandler(int param)
{
    // Tour->printElo();
    Tour->stop();
    Options->saveJson(Tour->getResults());

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
    try
    {
        Options = new CMD::Options(argc, argv);
        Tour = new Tournament(Options->getGameOptions());

        Tour->setResults(Options->getStats());

        Tour->startTournament(Options->getEngineConfigs());

        Options->saveJson(Tour->getResults());
    }
    catch (const std::runtime_error &e)
    {
        delete Tour;
        delete Options;

        throw e;

        exit(1);
    }

    delete Tour;
    delete Options;

    return 0;
}