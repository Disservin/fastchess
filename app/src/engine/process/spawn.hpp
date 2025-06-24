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

extern char **environ;

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

// https://git.musl-libc.org/cgit/musl/tree/src/process/execvp.c
// MIT License
inline int portable_execvpe(const char *file, char *const argv[], char *const envp[]) {
    const char *p, *z, *path = getenv("PATH");
    size_t l, k;
    int seen_eacces = 0;

    errno = ENOENT;
    if (!*file) return -1;

    if (strchr(file, '/')) return execve(file, argv, envp);

    if (!path) path = "/usr/local/bin:/bin:/usr/bin";
    k = strnlen(file, NAME_MAX + 1);
    if (k > NAME_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    l = strnlen(path, PATH_MAX - 1) + 1;

    for (p = path;; p = z) {
        auto b = std::make_unique<char[]>(l + k + 1);
        z      = strchrnul(p, ':');
        if (static_cast<size_t>(z - p) >= l) {
            if (!*z++) break;
            continue;
        }
        memcpy(b.get(), p, z - p);
        b[z - p] = '/';
        memcpy(b.get() + (z - p) + (z > p), file, k + 1);
        execve(b.get(), argv, envp);
        switch (errno) {
            case EACCES:
                seen_eacces = 1;
            case ENOENT:
            case ENOTDIR:
                break;
            default:
                return -1;
        }
        if (!*z++) break;
    }
    if (seen_eacces) errno = EACCES;
    return -1;
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

        portable_execvpe(command.c_str(), argv, environ);

        // If that failed and command doesn't contain '/', try in current directory
        if (command.find('/') == std::string::npos) {
            std::string local_command = "./" + command;
            execve(local_command.c_str(), argv, environ);
        }

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