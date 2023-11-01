#include <iostream>
#include <thread>

#include <cli.hpp>
#include <matchmaking/tournament.hpp>
#include <process/iprocess.hpp>
#include <process/process_list.hpp>

namespace fast_chess::atomic {
std::atomic_bool stop = false;
}  // namespace fast_chess::atomic

namespace fast_chess {

#ifdef _WIN64
#include <windows.h>
ProcessList<HANDLE> pid_list;
#else
#include <unistd.h>
ProcessList<pid_t> pid_list;
#endif
}  // namespace fast_chess

using namespace fast_chess;

namespace {
std::unique_ptr<cmd::OptionsParser> Options;
std::unique_ptr<Tournament> Tour;
}  // namespace

void clear_processes();
void setCtrlCHandler();

int main(int argc, char const *argv[]) {
    setCtrlCHandler();

    Logger::log<Logger::Level::TRACE>("Reading options...");
    Options = std::make_unique<cmd::OptionsParser>(argc, argv);

    Logger::log<Logger::Level::TRACE>("Creating tournament...");
    Tour = std::make_unique<Tournament>(Options->getGameOptions());

    Logger::log<Logger::Level::TRACE>("Setting results...");
    Tour->roundRobin()->setResults(Options->getResults());

    Logger::log<Logger::Level::TRACE>("Starting tournament...");
    Tour->start(Options->getEngineConfigs());

    Logger::log<Logger::Level::TRACE>("Saving results...");
    Options->saveJson(Tour->getResults());

    Logger::log("Saved results.");

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
    Logger::log<Logger::Level::INFO>("Saved results.");
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
        Logger::log<Logger::Level::FATAL>("Could not set control handler.");
        Logger::log<Logger::Level::INFO>("Saved results.");
    }
}

#else
void handler(int param) {
    consoleHandlerAction();
    std::exit(param);
}

void setCtrlCHandler() { signal(SIGINT, handler); }
#endif