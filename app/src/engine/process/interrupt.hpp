#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <features.h>  // Required for __GLIBC__

#if defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 9))
#    define CAN_USE_EVENTFD_FLAGS 1
#    include <sys/eventfd.h>
#else
#    define CAN_USE_EVENTFD_FLAGS 0
#endif

namespace fastchess::engine::process {
class InterruptSignaler {
   public:
    void setup() {
#ifdef CAN_USE_EVENTFD_FLAGS
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
            fcntl(fd_read_, F_SETFL, O_CLOEXEC);
        }
    }

    ~InterruptSignaler() {
        if (fd_read_ != -1) close(fd_read_);
        if (fd_write_ != -1 && fd_write_ != fd_read_) close(fd_write_);
    }

    int get_read_fd() const { return fd_read_; }
    int get_write_fd() const { return fd_write_; }

    static bool has_eventfd() {
#ifdef CAN_USE_EVENTFD_FLAGS
        return true;
#else
        return false;
#endif
    }

   private:
    int fd_read_  = -1;
    int fd_write_ = -1;
};
}  // namespace fastchess::engine::process
