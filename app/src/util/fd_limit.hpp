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
    }

    perror("getrlimit");
    return -1;
}
#else
[[nodiscard]] inline int maxSystemFileDescriptorCount() { return std::numeric_limits<int>::max(); }
#endif

[[nodiscard]] inline int minFileDescriptorRequired(int concurrency) noexcept { return 26 + (concurrency - 1) * 12; }

[[nodiscard]] inline int maxConcurrency(int availableFDs) noexcept { return (availableFDs - 26) / 12 + 1; }

}  // namespace fastchess::util::fd_limit
