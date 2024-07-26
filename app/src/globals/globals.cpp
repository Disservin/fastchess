#include <globals/globals.hpp>

#include <atomic>
#include <cassert>

#ifdef _WIN64
#    include <windows.h>
#else
#    include <signal.h>
#    include <unistd.h>
#    include <cstdlib>
#endif

#include <util/logger/logger.hpp>
#include <util/thread_vector.hpp>

namespace fastchess {

namespace atomic {
std::atomic_bool stop = false;
}  // namespace atomic

util::ThreadVector<ProcessInformation> process_list;

void writeToOpenPipes() {
    const auto nullbyte = '\0';

    process_list.lock();

    for (const auto &process : process_list) {
#ifdef _WIN64
        [[maybe_unused]] DWORD bytes_written;
        WriteFile(process.fd_write, &nullbyte, 1, &bytes_written, nullptr);
#else
        [[maybe_unused]] ssize_t bytes_written;
        bytes_written = write(process.fd_write, &nullbyte, 1);
#endif
        assert(bytes_written == 1);
    }

    process_list.unlock();
}

void stopProcesses() {
    process_list.lock();

    for (const auto &process : process_list) {
        Logger::trace("Cleaning up process with pid/handle: {}", process.identifier);

#ifdef _WIN64
        TerminateProcess(process.identifier, 1);
        CloseHandle(process.identifier);
#else
        kill(process.identifier, SIGINT);
        kill(process.identifier, SIGKILL);
#endif
    }

    process_list.unlock();
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

}  // namespace fastchess