#pragma once

#include <windows.h>
#include <cassert>
#include <map>
#include <memory>
#include <vector>

#include <affinity/cpuinfo/cpu_info.hpp>
#include <core/logger/logger.hpp>

namespace fastchess::affinity::cpu_info {

[[nodiscard]] inline int lsb(uint64_t bits) noexcept {
    assert(bits != 0);
#if defined(__GNUC__)
    return __builtin_ctzll(bits);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, bits);
    return static_cast<int>(idx);
#else
#    error "Compiler not supported."
#endif
}

[[nodiscard]] inline int pop(uint64_t& bits) noexcept {
    assert(bits != 0);
    int index = lsb(bits);
    bits &= bits - 1;
    return index;
}

inline CpuInfo getCpuInfo() noexcept(false) {
    LOG_TRACE("Getting CPU info");

    DWORD byte_length = 0;

    GetLogicalProcessorInformationEx(RelationProcessorPackage, nullptr, &byte_length);

    std::unique_ptr<char[]> buffer(new char[byte_length]);
    auto ptr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get());

    if (!GetLogicalProcessorInformationEx(RelationProcessorPackage, ptr, &byte_length)) {
        std::cerr << "GetLogicalProcessorInformationEx failed." << std::endl;
    }

    /*
     SMT Support = yes
     GroupCount = 1
     Group #0 = 0000000000000000000000000000000000000000000000000000000000000011

     SMT Support = yes
     GroupCount = 1
     Group #0 = 0000000000000000000000000000000000000000000000000000000000001100

     SMT Support = yes
     GroupCount = 1
     Group[0] = 0000000000000000000000000000000000000000000000000000000000110000
     */

    // @todo Make this work for multiple physical cpu's (multiple sockets and > 64 threads)
    int idx         = 0;
    int physical_id = 0;

    CpuInfo cpu_info;
    cpu_info.packages[physical_id].socket_id = physical_id;

    DWORD offset = 0;
    while (offset < byte_length) {
        if (ptr->Relationship == RelationProcessorPackage) {
            const int groupCount = ptr->Processor.GroupCount;

            for (int groupIdx = 0; groupIdx < groupCount; ++groupIdx) {
                auto mask = ptr->Processor.GroupMask[groupIdx].Mask;

                while (mask) {
                    const int processor = pop(mask) + groupIdx * 64;
                    // proper way to get this idx?
                    cpu_info.packages[physical_id].cores[idx].logical_processors.emplace_back(processor);
                }

                idx++;
            }
        }

        offset += ptr->Size;

        ptr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get() + offset);
    }

    return cpu_info;
}

}  // namespace fastchess::affinity::cpu_info
