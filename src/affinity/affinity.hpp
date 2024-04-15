#pragma once

#ifdef _WIN64
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#else
#include <sched.h>
#endif

namespace fast_chess {

namespace affinity {

#ifdef _WIN64

inline bool setAffinity(const std::vector<int>& cpus, HANDLE process_handle) noexcept {
    DWORD_PTR affinity_mask = 0;

    for (const auto& cpu : cpus) {
        assert(cpu <= 63);

        affinity_mask |= (1ull << cpu);
    }

    return SetProcessAffinityMask(process_handle, affinity_mask) != 0;
}

#elif defined(__APPLE__)

inline void setAffinity(const std::vector<int>&) noexcept {
    // mach_port_t tid = pthread_mach_thread_np(pthread_self());
    // struct thread_affinity_policy policy;
    // policy.affinity_tag = affinity_mask;

    // return thread_policy_set(tid, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
    //                          THREAD_AFFINITY_POLICY_COUNT);

    // do nothing for now, is affinity_tag supposed to be a mask or the core number?
}

#else

inline bool setAffinity(const std::vector<int>& cpus, pid_t process_pid) noexcept {
    cpu_set_t mask;
    CPU_ZERO(&mask);

    for (const auto& cpu : cpus) {
        CPU_SET(cpu, &mask);
    }

    return sched_setaffinity(process_pid, sizeof(cpu_set_t), &mask);
}

#endif
}  // namespace affinity

}  // namespace fast_chess