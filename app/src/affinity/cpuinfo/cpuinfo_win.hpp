#pragma once

#include <windows.h>
#include <array>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include <affinity/cpuinfo/cpu_info.hpp>
#include <core/logger/logger.hpp>

namespace fastchess::affinity::cpu_info {

[[nodiscard]] int lsb(uint64_t bits) noexcept {
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

[[nodiscard]] int pop(uint64_t& bits) noexcept {
    assert(bits != 0);
    int index = lsb(bits);
    bits &= bits - 1;
    return index;
}

inline CpuInfo getCpuInfo() noexcept(false) {
    Logger::trace("Getting CPU info");

    DWORD byte_length = 0;

    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &byte_length);

    std::unique_ptr<char[]> buffer(new char[byte_length]);
    auto ptr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get());

    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, ptr, &byte_length)) {
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
    cpu_info.physical_cpus[physical_id].physical_id = physical_id;

    DWORD offset = 0;
    while (offset < byte_length) {
        if (ptr->Relationship == RelationProcessorCore) {
            // If the PROCESSOR_RELATIONSHIP structure represents a processor core, the GroupCount
            // member is always 1.
            auto mask = ptr->Processor.GroupMask[0].Mask;

            while (mask) {
                const int processor = pop(mask);
                // proper way to get this idx?
                cpu_info.physical_cpus[physical_id].cores[idx].processors.emplace_back(processor);
            }

            idx++;
        }

        offset += ptr->Size;

        ptr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get() + offset);
    }

    return cpu_info;
}

}  // namespace fastchess::affinity::cpu_info