#pragma once

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <stdexcept>
#include <string>

#ifndef __APPLE__
extern char **environ;
#endif

namespace fastchess::engine::process {

inline void redirect_fd(int from_fd, int to_fd) {
    if (dup2(from_fd, to_fd) < 0) {
        perror("dup2 failed");
        _exit(127);
    }
}

inline void close_fd(int fd) { close(fd); }

inline void change_working_dir(const std::string &wd) {
    if (!wd.empty()) {
        if (chdir(wd.c_str()) != 0) {
            perror("chdir failed");
            _exit(127);
        }
    }
}

// 0 on success, non-zero on failure
inline int spawn_process(const std::string &command, char *const argv[], const std::string &working_dir, int stdin_fd,
                         int stdout_fd, int stderr_fd, pid_t &out_pid) {
    int status_pipe[2];
    if (pipe(status_pipe) != 0) {
        throw std::runtime_error("pipe failed");
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(status_pipe[0]);
        close(status_pipe[1]);
        throw std::runtime_error("fork failed");
    }

    if (pid == 0) {
        // --- Child ---
        // close read end
        close(status_pipe[0]);

        // Set pipe to close on exec
        fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);

        // File actions
        redirect_fd(stdin_fd, STDIN_FILENO);
        redirect_fd(stdout_fd, STDOUT_FILENO);
        redirect_fd(stderr_fd, STDERR_FILENO);

        change_working_dir(working_dir);

#ifdef __APPLE__
        char **&environ = *_NSGetEnviron();
        // environ         = envp_arr;
        execvp(command.c_str(), argv);
#else
        execvpe(command.c_str(), argv, environ);
#endif

        // If execvp failed
        const char err = '1';
        (void)!write(status_pipe[1], &err, 1);
        _exit(127);
    }

    // --- Parent ---
    // close write end
    close(status_pipe[1]);

    char result;
    ssize_t n = read(status_pipe[0], &result, 1);
    close(status_pipe[0]);

    if (n == 0) {
        // success
        out_pid = pid;
        return 0;
    }

    // failure
    // avoid zombie
    waitpid(pid, nullptr, 0);
    return 127;
}

}  // namespace fastchess::engine::process