#pragma once

#ifdef __WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <pthread.h>
#else
#include <sched.h>
#endif

#include <util/logger.hpp>

namespace affinity {

#ifdef __WIN32
#include <windows.h>
inline bool set_affinity(int core, HANDLE process_handle) {
    DWORD_PTR affinity_mask = 1ull << core;

    return SetProcessAffinityMask(process_handle, affinity_mask) != 0;
}
#elif defined(__APPLE__)

inline bool set_affinity(int core) {
    mach_port_t tid = pthread_mach_thread_np(pthread_self());
    struct thread_affinity_policy policy;
    policy.affinity_tag = core;

    return thread_policy_set(tid, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
                             THREAD_AFFINITY_POLICY_COUNT);
}

#else

inline bool set_affinity(int core, pid_t process_pid) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core, &mask);

    return sched_setaffinity(process_pid, sizeof(cpu_set_t), &mask);
}
#endif
}  // namespace affinity

namespace fast_chess {}  // namespace fast_chess