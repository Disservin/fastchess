#pragma once

// #include <signal.h>

namespace fast_chess {

struct ProcessInformation {
    int pid;
    int fd_write;
};

// Make sure that all processes are stopped, and no zombie processes are left after the
// program exits.
void triggerStop();
void stopProcesses();
void setCtrlCHandler();
}  // namespace fast_chess
