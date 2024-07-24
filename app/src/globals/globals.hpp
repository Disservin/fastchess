#pragma once

// #include <signal.h>
#ifdef _WIN64
#    include <windows.h>
#endif

namespace fast_chess {

struct ProcessInformation {
#ifdef _WIN64
    HANDLE pid;
    HANDLE fd_write;
#else
    int pid;
    int fd_write;
#endif
};

// Make sure that all processes are stopped, and no zombie processes are left after the
// program exits.
void triggerStop();
void stopProcesses();
void setCtrlCHandler();
}  // namespace fast_chess
