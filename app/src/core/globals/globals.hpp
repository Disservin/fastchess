#pragma once

// #include <signal.h>
#ifdef _WIN64
#    include <windows.h>
#endif

namespace fastchess {

/**
 * Information about the started processes (engines).
 */
struct ProcessInformation {
#ifdef _WIN64
    HANDLE identifier;
    HANDLE fd_write;
#else
    int identifier;
    int fd_write;
#endif
};

/**
 * When we receive a SIGINT (Ctrl+C) we want to stop all processes gracefully,
 * we also want to do this as quick as possible and without having to wait for
 * any timeout limits.
 * The best and most reliable way to do this (I think) is if we use a "pipe to self" trick.
 * Self is however perhaps the wrong word, we will write to a nullterminator the file descriptor
 * which the engine process is polling/reading from, these calls are otherwise blocking and will
 * not return until they have received something.
 * Approaches are described here:
 * https://stackoverflow.com/questions/43555398/correctly-processing-ctrl-c-when-using-poll A fancier approach is
 * described here, though lacks a windows counter part: https://mazzo.li/posts/stopping-linux-threads.html
 * At the end of the day, we want to make really sure we don't leave any processes hanging around (zombies).
 */

// Write a nullterminator to all engine processes, such that blocking read calls return.
// This is used to stop the engines gracefully, afterwards we can check the stop flag.
void writeToOpenPipes();

// Forcefully stop all processes, which we spawned for the engines.
void stopProcesses();

// Set the signal handler for SIGINT (Ctrl+C) to trigger the stop flag.
void setCtrlCHandler();
}  // namespace fastchess
