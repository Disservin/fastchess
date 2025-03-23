#include <core/globals/globals.hpp>

#include <cassert>

#ifdef _WIN64
#    include <windows.h>
#else
#    include <signal.h>
#    include <unistd.h>
#    include <cstdlib>
#endif

#include <core/logger/logger.hpp>
#include <core/threading/thread_vector.hpp>

namespace fastchess {

namespace atomic {
std::atomic_bool stop                 = false;
std::atomic_bool abnormal_termination = false;
}  // namespace atomic

util::ThreadVector<ProcessInformation> process_list;

void writeToOpenPipes() {
    const auto nullbyte = '\0';

    process_list.lock();

    for (const auto &process : process_list) {
        LOG_TRACE("Writing to process with pid/handle: {}", process.identifier);
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
        LOG_TRACE("Cleaning up process with pid/handle: {}", process.identifier);

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

void consoleHandlerAction() {
    Logger::print<Logger::Level::WARN>("Received signal, stopping tournament.");

    atomic::stop                 = true;
    atomic::abnormal_termination = true;
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
        LOG_FATAL("Could not set control handler.");
    }
}

#else
void handler(int) { consoleHandlerAction(); }

void setCtrlCHandler() { signal(SIGINT, handler); }
#endif

}  // namespace fastchess
