#pragma once

#include <array>
#include <cassert>
#include <string>

#include <fcntl.h>   // fcntl
#include <unistd.h>  // close, pipe

#include <types/exception.hpp>

namespace fastchess::engine::process {
struct Pipe {
    std::array<int, 2> fds_;

    Pipe() { initialize(); }

    Pipe(const Pipe &) { initialize(); }

    Pipe &operator=(const Pipe &) {
        close_read_end();
        close_write_end();
        initialize();
        return *this;
    }

    ~Pipe() {
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
        if (pipe(fds_.data()) != 0) throw fastchess_exception("pipe() failed");
        if (fcntl(fds_[0], F_SETFD, FD_CLOEXEC) == -1 || fcntl(fds_[1], F_SETFD, FD_CLOEXEC) == -1)
            throw fastchess_exception("fcntl() failed");
    }
};
}  // namespace fastchess::engine::process