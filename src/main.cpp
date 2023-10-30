#include <iostream>
#include <thread>

#include <cli.hpp>
#include <matchmaking/tournament.hpp>
#include <third_party/communication.hpp>

namespace fast_chess::atomic {
std::atomic_bool stop = false;
}  // namespace fast_chess::atomic

using namespace fast_chess;

namespace {
std::unique_ptr<cmd::OptionsParser> Options;
std::unique_ptr<Tournament> Tour;
}  // namespace

void clear_processes();
void setCtrlCHandler();

int main(int argc, char const *argv[]) {
    setCtrlCHandler();

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

    std::cout << "Saved results." << std::endl;

    clear_processes();

    return 0;
}

void clear_processes() {
#ifdef _WIN64
    for (const auto &pid : pid_list) {
        TerminateProcess(pid, 1);
        CloseHandle(pid);
    }
#else
    for (const auto &pid : pid_list) {
        kill(pid, SIGINT);
        kill(pid, SIGKILL);
    }
#endif
}

void consoleHandlerAction() {
    Tour->stop();
    Options->saveJson(Tour->getResults());
    clear_processes();
    std::cout << "Saved results" << std::endl;
    std::exit(0);
}

#ifdef _WIN64
BOOL WINAPI handler(DWORD signal) {
    switch (signal) {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        case CTRL_C_EVENT:
            consoleHandlerAction();
        default:
            break;
    }
    return FALSE;
}

void setCtrlCHandler() {
    if (!SetConsoleCtrlHandler(handler, TRUE)) {
        std::cout << "\nERROR: Could not set control handler\n";
    }
}

#else
void handler(int param) {
    consoleHandlerAction();
    std::exit(param);
}

void setCtrlCHandler() { signal(SIGINT, handler); }
#endif