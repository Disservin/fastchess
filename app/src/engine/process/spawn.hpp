#pragma once

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/prctl.h>

#include <string>

#ifndef __APPLE__
extern char **environ;
#else
#include <crt_externs.h>  // for _NSGetEnviron
#endif

namespace fastchess::engine::process {

inline void redirect_fd(int from_fd, int to_fd) {
    if (dup2(from_fd, to_fd) < 0) {
        _exit(127);
    }
}

inline void close_fd(int fd) { close(fd); }

inline void change_working_dir(const std::string &wd) {
    if (!wd.empty()) {
        if (chdir(wd.c_str()) != 0) {
            _exit(127);
        }
    }
}

// Returns 0 on success, errno value on failure
inline int spawn_process(const std::string &command, char *const argv[], const std::string &working_dir, int stdin_fd,
                         int stdout_fd, int stderr_fd, pid_t &out_pid, bool use_execve = false) {
    int status_pipe[2];
    int ec = 0;

    if (pipe(status_pipe) != 0) {
        return errno;
    }

    fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);

    sigset_t oldmask, blockset;
    sigfillset(&blockset);
    sigprocmask(SIG_BLOCK, &blockset, &oldmask);


    pid_t pid = fork();
    if (pid < 0) {
        ec = errno;
        close(status_pipe[0]);
        close(status_pipe[1]);
        sigprocmask(SIG_SETMASK, &oldmask, nullptr);
        return ec;
    }


    if (pid == 0) {
        // --- Child ---
        int ret = 0;
        int p   = status_pipe[1];

        prctl(PR_SET_PDEATHSIG, SIGKILL);

        close(status_pipe[0]);

        struct sigaction sa = {};
        sa.sa_handler       = SIG_DFL;
        for (int i = 1; i < NSIG; i++) {
            sigaction(i, &sa, nullptr);
        }

        if (stdin_fd == p) {
            ret = dup(p);
            if (ret < 0) goto fail;
            close(p);
            p = ret;
        }

        if (stdout_fd == p) {
            ret = dup(p);
            if (ret < 0) goto fail;
            close(p);
            p = ret;
        }

        if (stderr_fd == p) {
            ret = dup(p);
            if (ret < 0) goto fail;
            close(p);
            p = ret;
        }

        if ((ret = dup2(stdin_fd, STDIN_FILENO)) < 0) goto fail;
        if ((ret = dup2(stdout_fd, STDOUT_FILENO)) < 0) goto fail;
        if ((ret = dup2(stderr_fd, STDERR_FILENO)) < 0) goto fail;

        if (!working_dir.empty()) {
            if (chdir(working_dir.c_str()) != 0) {
                ret = -errno;
                goto fail;
            }
        }

        fcntl(p, F_SETFD, FD_CLOEXEC);

        sigprocmask(SIG_SETMASK, &oldmask, nullptr);

        if (use_execve) {
            #ifdef __APPLE__
            char**& environ = *_NSGetEnviron(); 
            #endif
            execve(command.c_str(), argv, environ);
        } else {
#ifdef __APPLE__
            // macos environment does not support execvp
            execvp(command.c_str(), argv);
#else
            execvpe(command.c_str(), argv, environ);
#endif
        }

        ret = -errno;

    fail:
        ret = -ret;

        if (ret)
            while (write(p, &ret, sizeof(ret)) < 0);

        _exit(127);
    }

    // --- Parent ---
    close(status_pipe[1]);


    int child_errno = 0;
    ssize_t n       = read(status_pipe[0], &child_errno, sizeof(child_errno));

    if (pid > 0) {
        if (n != sizeof(child_errno)) {
            child_errno = 0;
        } else {
            waitpid(pid, nullptr, 0);
            ec = child_errno;
        }
    } else {
        ec = errno;
    }

    close(status_pipe[0]);

    sigprocmask(SIG_SETMASK, &oldmask, nullptr);

    if (!ec) {
        out_pid = pid;
        return 0;
    }

    return ec;
}

}  // namespace fastchess::engine::process