#pragma once

#include <vector>
#include <array>
#include <thread>
#include <windows.h>

namespace fast_chess {

inline std::array<std::vector<int>, 2> get_physical_cores() {
    std::vector<int> ht_1;
    std::vector<int> ht_2;

    DWORD byte_length = 0;

    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &byte_length);

    std::unique_ptr<char[]> buffer(new char[byte_length]);

    if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
                                          PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get()),
                                          &byte_length)) {
        std::cerr << "GetLogicalProcessorInformationEx failed." << std::endl;
    }

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ptr =
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get());

    DWORD offset = 0;

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
    while (offset < byte_length) {
        if (ptr->Relationship == RelationProcessorCore) {
            // If the PROCESSOR_RELATIONSHIP structure represents a processor core, the GroupCount
            // member is always 1.
            ULONG_PTR mask = ptr->Processor.GroupMask[0].Mask;

            int idx = 0;

            while (mask) {
                const int core = chess::builtin::poplsb(mask);
                std::cout << "core: " << core << std::endl;
                if (idx % 2 == 0) {
                    ht_1.push_back(core);
                } else {
                    ht_2.push_back(core);
                }

                idx++;
            }
        }

        offset += ptr->Size;

        ptr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get() + offset);
    }

    return {ht_1, ht_2};
}
}  // namespace fast_chess