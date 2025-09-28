#pragma once

#include <core/logger/logger.hpp>

#ifdef _WIN64
#    include <processthreadsapi.h>
#    include <windows.h>
#elif defined(__APPLE__)
#    include <mach/thread_act.h>
#    include <mach/thread_policy.h>
#    include <pthread.h>
#else
#    include <sched.h>
#    include <unistd.h>
#endif

namespace fastchess {

namespace affinity {

#ifdef _WIN64

namespace detail {

template <typename F>
[[nodiscard]] F getWindowsApi(const char* func_name) {
    const HMODULE hModule = GetModuleHandle(TEXT("kernel32.dll"));
    if (!hModule) {
        return nullptr;
    }

#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
    const auto pFunc = reinterpret_cast<F>(GetProcAddress(hModule, func_name));
#    pragma GCC diagnostic pop

    return pFunc;
}

[[nodiscard]] inline std::vector<GROUP_AFFINITY> getGroupAffinities(const std::vector<int>& cpus) {
    // Create an array of GROUP_AFFINITY structures
    std::vector<GROUP_AFFINITY> groupAffinities(cpus.size());

    for (size_t i = 0; i < cpus.size(); ++i) {
        GROUP_AFFINITY& ga = groupAffinities[i];
        ZeroMemory(&ga, sizeof(GROUP_AFFINITY));
        ga.Mask  = 1ull << (cpus[i] % 64);           // Assuming cpus[i] is the logical processor number
        ga.Group = static_cast<WORD>(cpus[i] / 64);  // Assuming each group has 64 processors
    }

    return groupAffinities;
}

}  // namespace detail

[[nodiscard]] inline bool setThreadAffinity(const std::vector<int>& cpus, HANDLE thread_handle) noexcept {
    LOG_TRACE("Setting affinity mask for thread handle: {}", thread_handle);
    using SetThreadSelectedCpuSetMasksFunc = BOOL(WINAPI*)(HANDLE, PGROUP_AFFINITY, USHORT);

    // Check if SetThreadSelectedCpuSetMasks is available (Windows 11+)
    const auto pSetThreadSelectedCpuSetMasks =
        detail::getWindowsApi<SetThreadSelectedCpuSetMasksFunc>("SetThreadSelectedCpuSetMasks");
    if (pSetThreadSelectedCpuSetMasks) {
        std::vector<GROUP_AFFINITY> groupAffinities = detail::getGroupAffinities(cpus);

        // Set the CPU set masks for the thread
        return pSetThreadSelectedCpuSetMasks(thread_handle, groupAffinities.data(),
                                             static_cast<USHORT>(groupAffinities.size())) != 0;
    }

    // Fallback to SetThreadAffinityMask for older Windows versions (limited to 64 CPUs)
    DWORD_PTR affinity_mask = 0;
    for (const auto& cpu : cpus) {
        if (cpu > 63) {
            Logger::print<Logger::Level::ERR>(
                "Setting thread affinity for more than 64 logical CPUs is not supported: requires at least Windows 11 "
                "or Windows Server 2022.");
            return false;
        }
        affinity_mask |= (1ull << cpu);
    }

    return SetThreadAffinityMask(thread_handle, affinity_mask) != 0;
}

[[nodiscard]] inline bool setProcessAffinity(const std::vector<int>& cpus, HANDLE process_handle) noexcept {
    LOG_TRACE("Setting affinity mask for process handle: {}", process_handle);
    using SetProcessDefaultCpuSetMasksFunc = BOOL(WINAPI*)(HANDLE, PGROUP_AFFINITY, USHORT);

    // Check if SetProcessDefaultCpuSetMasks is available
    const auto pSetProcessDefaultCpuSetMasks =
        detail::getWindowsApi<SetProcessDefaultCpuSetMasksFunc>("SetProcessDefaultCpuSetMasks");
    if (pSetProcessDefaultCpuSetMasks) {
        std::vector<GROUP_AFFINITY> groupAffinities = detail::getGroupAffinities(cpus);

        // Set the CPU set masks for the process
        return pSetProcessDefaultCpuSetMasks(process_handle, groupAffinities.data(),
                                             static_cast<USHORT>(groupAffinities.size())) == TRUE;
    }

    // Fallback to SetProcessAffinityMask for older Windows versions
    DWORD_PTR affinity_mask = 0;

    for (const auto& cpu : cpus) {
        if (cpu > 63) {
            Logger::print<Logger::Level::ERR>(
                "Setting affinity for more than 64 logical CPUs is not supported: requires at least Windows 11 or "
                "Windows Server 2022.");
            return false;
        }
        affinity_mask |= (1ull << cpu);
    }

    return SetProcessAffinityMask(process_handle, affinity_mask) != 0;
}

#elif defined(__APPLE__)

[[nodiscard]] inline bool setThreadAffinity(const std::vector<int>&, pid_t) noexcept {
    // Not implemented.
    return false;
}

[[nodiscard]] inline bool setProcessAffinity(const std::vector<int>&, pid_t) noexcept {
    // mach_port_t tid = pthread_mach_thread_np(pthread_self());
    // struct thread_affinity_policy policy;
    // policy.affinity_tag = affinity_mask;

    // return thread_policy_set(tid, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
    //                          THREAD_AFFINITY_POLICY_COUNT);

    // do nothing for now, is affinity_tag supposed to be a mask or the core number?
    return false;
}

#else

[[nodiscard]] inline bool setThreadAffinity(const std::vector<int>& cpus, pid_t thread_pid) noexcept {
    LOG_TRACE("Setting affinity mask for thread pid: {}", thread_pid);

    cpu_set_t mask;
    CPU_ZERO(&mask);

    for (const auto& cpu : cpus) {
        CPU_SET(cpu, &mask);
    }

    return sched_setaffinity(thread_pid, sizeof(cpu_set_t), &mask) == 0;
}

[[nodiscard]] inline bool setProcessAffinity(const std::vector<int>& cpus, pid_t process_pid) noexcept {
    // Set the affinity for the process by setting the affinity for its main thread.
    return setThreadAffinity(cpus, process_pid);
}
#endif

#ifdef _WIN64
[[nodiscard]] inline HANDLE getProcessHandle() noexcept { return GetCurrentProcess(); }
#else
[[nodiscard]] inline pid_t getProcessHandle() noexcept { return getpid(); }
#endif

#ifdef _WIN64
[[nodiscard]] inline HANDLE getThreadHandle() noexcept { return GetCurrentThread(); }
#else
[[nodiscard]] inline pid_t getThreadHandle() noexcept {
#    ifdef __APPLE__
    // dummy
    return 0;
#    else
    return gettid();
#    endif
}
#endif
}  // namespace affinity

}  // namespace fastchess