#pragma once

#ifndef _WIN64
#    include <stdio.h>
#    include <sys/resource.h>
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

[[nodiscard]] inline int minFileDescriptorRequired(int concurrency) noexcept { return 26 + (concurrency - 1) * 12; }

[[nodiscard]] inline int maxConcurrency(int availableFDs) noexcept { return (availableFDs - 26) / 12 + 1; }

}  // namespace fastchess::fd_limit
