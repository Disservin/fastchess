#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engines/uci_engine.hpp"
#include "matchmaking/tournament.hpp"
#include "options.hpp"

namespace fast_chess {
namespace Atomic {
std::atomic_bool stop = false;
}  // namespace Atomic
}  // namespace fast_chess

using namespace fast_chess;

namespace {
std::unique_ptr<CMD::Options> Options;
std::unique_ptr<Tournament> Tour;
}  // namespace

#ifdef _WIN64

BOOL WINAPI consoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        case CTRL_C_EVENT:
            Tour->stop();
            std::cout << "Saved results" << std::endl;

            return FALSE;
        default:
            break;
    }

    return FALSE;
}

#else
void sigintHandler(int param) {
    Options->saveJson(Tour->results());

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

        Tour->start(Options->getEngineConfigs());

        std::cout << "Saved results" << std::endl;
    } catch (const std::exception &e) {
        throw e;
    }

    return 0;
}