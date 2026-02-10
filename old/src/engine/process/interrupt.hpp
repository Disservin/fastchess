#pragma once

#include <fcntl.h>
#include <unistd.h>

#ifdef __linux
#    include <features.h>  // Required for __GLIBC__
#endif

#if defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 9))
#    define CAN_USE_EVENTFD_FLAGS 1
#    include <sys/eventfd.h>
#else
#    define CAN_USE_EVENTFD_FLAGS 0
#endif

namespace fastchess::engine::process {
class InterruptSignaler {
   public:
    InterruptSignaler() = default;

    InterruptSignaler(const InterruptSignaler&)            = delete;
    InterruptSignaler& operator=(const InterruptSignaler&) = delete;

    InterruptSignaler(InterruptSignaler&& other) noexcept : fd_read_(other.fd_read_), fd_write_(other.fd_write_) {
        other.fd_read_  = -1;
        other.fd_write_ = -1;
    }

    InterruptSignaler& operator=(InterruptSignaler&& other) noexcept {
        if (this != &other) {
            this->cleanup();

            fd_read_  = other.fd_read_;
            fd_write_ = other.fd_write_;

            other.fd_read_  = -1;
            other.fd_write_ = -1;
        }

        return *this;
    }

    ~InterruptSignaler() {
        if (fd_read_ != -1) close(fd_read_);
        if (fd_write_ != -1 && fd_write_ != fd_read_) close(fd_write_);
    }

    void setup() {
#if CAN_USE_EVENTFD_FLAGS
        fd_read_ = eventfd(0, EFD_CLOEXEC);
        if (fd_read_ != -1) {
            // eventfd uses same FD for both
            fd_write_ = fd_read_;
            return;
        }
#endif
        int pipefds[2];
        if (pipe(pipefds) == 0) {
            fd_read_  = pipefds[0];
            fd_write_ = pipefds[1];
            fcntl(fd_read_, F_SETFD, FD_CLOEXEC);
            fcntl(fd_write_, F_SETFD, FD_CLOEXEC);
        }
    }

    void cleanup() {
        if (fd_read_ != -1) {
            close(fd_read_);
        }
        // Only close write side if it's a pipe (different from read side)
        if (fd_write_ != -1 && fd_write_ != fd_read_) {
            close(fd_write_);
        }
        fd_read_  = -1;
        fd_write_ = -1;
    }

    int get_read_fd() const { return fd_read_; }
    int get_write_fd() const { return fd_write_; }

    bool has_eventfd() const {
        // eventfd uses the same file descriptor for both read and write.
        // When eventfd() fails or is unavailable, setup() falls back to pipe(),
        // which uses two distinct file descriptors.
        return fd_read_ != -1 && fd_read_ == fd_write_;
    }

   private:
    int fd_read_  = -1;
    int fd_write_ = -1;
};
}  // namespace fastchess::engine::process
