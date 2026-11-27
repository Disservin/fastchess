#pragma once

#include <array>
#include <cassert>
#include <string>

#include <fcntl.h>   // fcntl, O_CLOEXEC
#include <unistd.h>  // close, pipe, pipe2

#include <types/exception.hpp>

namespace fastchess::engine::process {

// Creates a pipe whose two ends are close-on-exec, so that no engine process inherits them. With pipe() then
// fcntl(FD_CLOEXEC), an engine that another thread spawns between the two calls inherits both ends and keeps
// them open: when the engine they belong to exits, the read of its output never sees the end. pipe2() sets the
// flag atomically; macOS has no pipe2(), so the race remains there.
inline int cloexec_pipe(int fds[2]) noexcept {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    return pipe2(fds, O_CLOEXEC);
#else
    if (pipe(fds) != 0) return -1;
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    return 0;
#endif
}

struct Pipe {
    std::array<int, 2> fds_;

    Pipe() { initialize(); }

    Pipe(const Pipe&) = delete;

    Pipe& operator=(const Pipe&) {
        close_read_end();
        close_write_end();
        initialize();
        return *this;
    }

    ~Pipe() { close_fds(); }

    void close_fds() {
        close_read_end();
        close_write_end();
    }

    int read_end() const {
        assert(fds_[0] != -1);
        return fds_[0];
    }
    int write_end() const {
        assert(fds_[1] != -1);
        return fds_[1];
    }

    void close_read_end() {
        if (fds_[0] == -1) return;
        close(fds_[0]);
        fds_[0] = -1;
    }

    void close_write_end() {
        if (fds_[1] == -1) return;
        close(fds_[1]);
        fds_[1] = -1;
    }

   private:
    void initialize() {
        if (cloexec_pipe(fds_.data()) != 0) throw fastchess_exception("pipe() failed");
    }
};
}  // namespace fastchess::engine::process