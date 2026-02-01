#pragma once

#ifndef _WIN64
#    include <stdio.h>
#    include <sys/resource.h>
#    include <engine/process/interrupt.hpp>
#else
#    include <windows.h>
#endif

namespace fastchess::fd_limit {

#ifndef _WIN64
[[nodiscard]] inline int maxSystemFileDescriptorCount() {
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        return limit.rlim_cur;
    }

    perror("getrlimit");
    return -1;
}

#else
[[nodiscard]] inline bool isWindows11OrNewer() {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    DWORDLONG conditionMask = 0;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    osvi.dwMajorVersion = 10;
    osvi.dwMinorVersion = 0;
    osvi.dwBuildNumber  = 22000;  // Windows 11 starts from build 22000

    return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, conditionMask);
}
#endif

#ifdef CAN_USE_EVENTFD_FLAGS
[[nodiscard]] inline int minFileDescriptorRequired(int concurrency) noexcept { return 12 + concurrency * 10; }

[[nodiscard]] inline int maxConcurrency(int availableFDs) noexcept { return (availableFDs - 12) / 10; }

#else

[[nodiscard]] inline int minFileDescriptorRequired(int concurrency) noexcept { return 14 + (concurrency) * 12; }

[[nodiscard]] inline int maxConcurrency(int availableFDs) noexcept { return (availableFDs - 14) / 12; }
#endif

}  // namespace fastchess::fd_limit
