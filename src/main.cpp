#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <cli.hpp>
#include <engines/uci_engine.hpp>
#include <matchmaking/tournament.hpp>

namespace fast_chess {
namespace atomic {
std::atomic_bool stop = false;
}  // namespace atomic
}  // namespace fast_chess

using namespace fast_chess;

namespace {
std::unique_ptr<cmd::OptionsParser> Options;
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
            Options->saveJson(Tour->getResults());
            std::cout << "Saved results" << std::endl;
            exit(0);
        default:
            break;
    }

    return FALSE;
}

#else
void sigintHandler(int param) {
    Tour->stop();
    Options->saveJson(Tour->getResults());
    std::cout << "Saved results" << std::endl;

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
    Logger::debug("Reading options...");
    Options = std::make_unique<cmd::OptionsParser>(argc, argv);

    Logger::debug("Creating tournament...");
    Tour = std::make_unique<Tournament>(Options->getGameOptions());

    Logger::debug("Setting results...");
    Tour->roundRobin()->setResults(Options->getResults());

    Logger::debug("Starting tournament...");
    Tour->start(Options->getEngineConfigs());

    Logger::debug("Saving results...");
    Options->saveJson(Tour->getResults());

    std::cout << "Saved results" << std::endl;

    return 0;
}