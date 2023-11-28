#include <thread>

#include <cli.hpp>
#include <matchmaking/tournament.hpp>
#include <util/thread_vector.hpp>

namespace fast_chess {

namespace atomic {
std::atomic_bool stop = false;
}

#ifdef _WIN64
#include <windows.h>
ThreadVector<HANDLE> process_list;
#else
#include <unistd.h>
ThreadVector<pid_t> process_list;
#endif
}  // namespace fast_chess

using namespace fast_chess;

void stopProcesses();
void setCtrlCHandler();

int main(int argc, char const *argv[]) {
    setCtrlCHandler();

    Logger::log<Logger::Level::TRACE>("Reading options...");
    auto options = cmd::OptionsParser(argc, argv);

    {
        Logger::log<Logger::Level::TRACE>("Creating tournament...");
        auto tour = Tournament(options.getGameOptions(), options.getEngineConfigs());

        Logger::log<Logger::Level::TRACE>("Setting results...");
        tour.roundRobin()->setResults(options.getResults());

        Logger::log<Logger::Level::TRACE>("Starting tournament...");
        tour.start();

        Logger::log<Logger::Level::INFO>("Finished tournament.");
    }

    stopProcesses();

    return 0;
}

/// @brief Make sure that all processes are stopped, and no zombie processes are left after the
/// program exits.
void stopProcesses() {
#ifdef _WIN64
    for (const auto &pid : process_list) {
        TerminateProcess(pid, 1);
        CloseHandle(pid);
    }
#else
    for (const auto &pid : process_list) {
        kill(pid, SIGINT);
        kill(pid, SIGKILL);
    }
#endif
}

void consoleHandlerAction() { fast_chess::atomic::stop = true; }

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
    return TRUE;
}

void setCtrlCHandler() {
    if (!SetConsoleCtrlHandler(handler, TRUE)) {
        Logger::log<Logger::Level::FATAL>("Could not set control handler.");
    }
}

#else
void handler(int param) {
    consoleHandlerAction();
    std::exit(param);
}

void setCtrlCHandler() { signal(SIGINT, handler); }
#endif