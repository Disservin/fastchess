#pragma once

#ifndef _WIN32
#    include <stdio.h>
#    include <sys/resource.h>
#else
#    include <limits>
#endif

namespace fastchess::util::fd_limit {

#ifndef _WIN32
[[nodiscard]] inline int maxSystemFileDescriptorCount() {
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        return limit.rlim_cur;
    } else {
        perror("getrlimit");
        return -1;
    }
}
#else
[[nodiscard]] inline int maxSystemFileDescriptorCount() { return std::numeric_limits<int>::max(); }
#endif

inline int minFileDescriptorRequired(int concurrency) { return 26 + (concurrency - 1) * 12; }

inline int maxConcurrency(int availableFDs) { return (availableFDs - 26) / 12 + 1; }

}  // namespace fastchess::util::fd_limit
