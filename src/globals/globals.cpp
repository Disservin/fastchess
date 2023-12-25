#include <globals/globals.hpp>

#include <util/logger/logger.hpp>
#include <util/thread_vector.hpp>

namespace fast_chess {

#include <atomic>

namespace atomic {
std::atomic_bool stop = false;
}

#ifdef _WIN64
#include <windows.h>
ThreadVector<HANDLE> process_list;
#else
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
ThreadVector<pid_t> process_list;
#endif

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

}  // namespace fast_chess