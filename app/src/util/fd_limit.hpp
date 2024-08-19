#pragma once

#ifndef _WIN32
#    include <stdio.h>
#    include <sys/resource.h>
#else
#    include <limits>
#endif

namespace fastchess::util {

#ifndef _WIN32
[[nodiscard]] inline int maxFileDescriptors() {
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        return limit.rlim_cur;
    } else {
        perror("getrlimit");
        return -1;
    }
}
#else
[[nodiscard]] inline int maxFileDescriptors() { return return std::numeric_limits<int>::max(); }
#endif

}  // namespace fastchess::util
