#pragma once

#include <cassert>
#include <sstream>
#include <string>

#include <fcntl.h>  // fcntl
#include <poll.h>   // poll
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>  // pid_t
#include <sys/wait.h>

#include <types/exception.hpp>

namespace fastchess::engine::process {

namespace detail {
inline std::string termSignalToString(int sig) {
    switch (sig) {
        case SIGABRT:
            return "SIGABRT - Abort";
        case SIGALRM:
            return "SIGALRM - Alarm clock";
        case SIGBUS:
            return "SIGBUS - Bus error";
        case SIGCHLD:
            return "SIGCHLD - Child stopped or terminated";
        case SIGCONT:
            return "SIGCONT - Continue executing";
        case SIGFPE:
            return "SIGFPE - Floating point exception";
        case SIGHUP:
            return "SIGHUP - Hangup";
        case SIGILL:
            return "SIGILL - Illegal instruction";
        case SIGINT:
            return "SIGINT - Interrupt";
        case SIGKILL:
            return "SIGKILL - Kill";
        case SIGPIPE:
            return "SIGPIPE - Broken pipe";
        case SIGQUIT:
            return "SIGQUIT - Quit program";
        case SIGSEGV:
            return "SIGSEGV - Segmentation fault";
        case SIGSTOP:
            return "SIGSTOP - Stop executing";
        case SIGTERM:
            return "SIGTERM - Termination";
        case SIGTRAP:
            return "SIGTRAP - Trace/breakpoint trap";
        case SIGTSTP:
            return "SIGTSTP - Terminal stop signal";
        case SIGTTIN:
            return "SIGTTIN - Background process attempting read";
        case SIGTTOU:
            return "SIGTTOU - Background process attempting write";
        case SIGUSR1:
            return "SIGUSR1 - User-defined signal 1";
        case SIGUSR2:
            return "SIGUSR2 - User-defined signal 2";
#ifdef SIGPOLL
        case SIGPOLL:
            return "SIGPOLL - Pollable event";
#endif
        case SIGPROF:
            return "SIGPROF - Profiling timer expired";
        case SIGSYS:
            return "SIGSYS - Bad system call";
        case SIGURG:
            return "SIGURG - Urgent condition on socket";
        case SIGVTALRM:
            return "SIGVTALRM - Virtual timer expired";
        case SIGXCPU:
            return "SIGXCPU - CPU time limit exceeded";
        case SIGXFSZ:
            return "SIGXFSZ - File size limit exceeded";
        default:
            return "Unknown signal";
    }
}

inline std::string stopSignalToString(int sig) {
    switch (sig) {
        case SIGSTOP:
            return "SIGSTOP - Stop executing";
        case SIGTSTP:
            return "SIGTSTP - Terminal stop signal";
        case SIGTTIN:
            return "SIGTTIN - Background process attempting read";
        case SIGTTOU:
            return "SIGTTOU - Background process attempting write";
        default:
            return "Unknown stop signal";
    }
}

}  // namespace detail

inline std::string signalToString(int status) {
    std::stringstream result;

    if (WIFEXITED(status)) {
        result << "Process exited normally with status " << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result << "Process terminated by signal " << WTERMSIG(status) << " (";
        result << detail::termSignalToString(WTERMSIG(status));
        result << ")";

#ifdef WCOREDUMP
        if (WCOREDUMP(status)) {
            result << " - Core dumped";
        }
#endif
    } else if (WIFSTOPPED(status)) {
        result << "Process stopped by signal " << WSTOPSIG(status);
        result << " (" << detail::stopSignalToString(WSTOPSIG(status)) << ")";
    }
    // Not all systems define this
#ifdef WIFCONTINUED
    else if (WIFCONTINUED(status)) {
        result << "Process continued";
    }
#endif
    else {
        result << "Unknown status " << status;
    }

    return result.str();
}

}  // namespace fastchess::engine::process