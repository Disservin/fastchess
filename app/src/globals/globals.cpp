#include <globals/globals.hpp>

#include <util/logger/logger.hpp>
#include <util/thread_vector.hpp>

namespace fast_chess {

#include <atomic>

namespace atomic {
std::atomic_bool stop = false;
}

#ifdef _WIN64
#    include <windows.h>
util::ThreadVector<HANDLE> process_list;
#else
#    include <signal.h>
#    include <unistd.h>
#    include <cstdlib>
util::ThreadVector<pid_t> process_list;
#endif

void stopProcesses() {
#ifdef _WIN64
    for (const auto &pid : process_list) {
        Logger::trace("Terminating process {}", pid);
        TerminateProcess(pid, 1);
        Logger::trace("Closing handle for process {}", pid);
        CloseHandle(pid);
    }
#else
    for (const auto &pid : process_list) {
        kill(pid, SIGINT);
        kill(pid, SIGKILL);
    }
#endif
}

void consoleHandlerAction() { atomic::stop = true; }

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
        Logger::fatal("Could not set control handler.");
    }
}

#else
void handler(int) { consoleHandlerAction(); }

void setCtrlCHandler() { signal(SIGINT, handler); }
#endif

}  // namespace fast_chess