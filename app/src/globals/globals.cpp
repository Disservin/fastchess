#include <globals/globals.hpp>

#include <atomic>
#include <cassert>

#include <util/logger/logger.hpp>
#include <util/thread_vector.hpp>

namespace fast_chess {

namespace atomic {
std::atomic_bool stop   = false;
std::atomic_bool signal = false;
}  // namespace atomic

#ifdef _WIN64
#    include <windows.h>
util::ThreadVector<ProcessInformation> process_list;
#else
#    include <signal.h>
#    include <unistd.h>
#    include <cstdlib>
util::ThreadVector<ProcessInformation> process_list;
#endif

void triggerStop() {
    const auto nullbyte = '\0';

#ifdef _WIN64

    for (const auto &pid : process_list) {
        [[maybe_unused]] DWORD bytesWritten;
        WriteFile(pid.fd_write, &nullbyte, 1, &bytesWritten, nullptr);
        assert(bytesWritten == 1);
    }
#else
    for (const auto &pid : process_list) {
        [[maybe_unused]] ssize_t res;
        res = write(pid.fd_write, &nullbyte, 1);
        assert(res == 1);
    }
#endif
}

void stopProcesses() {
#ifdef _WIN64
    for (const auto &pid : process_list) {
        Logger::trace("Terminating process {}", pid.pid);
        TerminateProcess(pid.pid, 1);
        Logger::trace("Closing handle for process {}", pid.pid);
        CloseHandle(pid.pid);
    }
#else
    for (const auto &pid : process_list) {
        kill(pid.pid, SIGINT);
        kill(pid.pid, SIGKILL);
    }
#endif
}

void consoleHandlerAction() {
    atomic::stop   = true;
    atomic::signal = true;
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