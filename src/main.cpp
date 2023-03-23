#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engines/uci_engine.hpp"
#include "matchmaking/tournament.hpp"
#include "options.hpp"

using namespace fast_chess;

namespace {
std::unique_ptr<Tournament> Tour;
std::unique_ptr<CMD::Options> Options;
}  // namespace

#ifdef _WIN64

BOOL WINAPI consoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        case CTRL_C_EVENT:

            std::cout << "Saved results" << std::endl;
            Options->saveJson(Tour->getResults());

            Tour->stop();

            return TRUE;
        default:
            break;
    }

    return FALSE;
}

#else
void sigintHandler(int param) {
    Options->saveJson(Tour->getResults());
    Tour->stop();

    exit(param);
}

#endif

int main(int argc, char const *argv[]) {
#ifdef _WIN64
    if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
        std::cout << "\nERROR: Could not set control handler\n";
        return 1;
    }

#else
    signal(SIGINT, sigintHandler);
#endif
    try {
        Options = std::make_unique<CMD::Options>(argc, argv);
        Tour = std::make_unique<Tournament>(Options->getGameOptions());

        Tour->setResults(Options->getStats());

        Tour->startTournament(Options->getEngineConfigs());

        std::cout << "Saved results" << std::endl;
        Options->saveJson(Tour->getResults());
    } catch (const std::exception &e) {
        throw e;
    }

    return 0;
}