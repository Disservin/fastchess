#pragma once

#include <vector>

namespace affinity {

/// @brief Contains information about the cpu's on the system.
struct CpuInfo {
    struct PhysicalCpu {
        struct Core {
            struct Processor {
                Processor(int processor_id) : processor_id(processor_id) {}
                int processor_id;
            };

            /// @brief Processor's in this core. Probably 2. Can be 1 on non-HT systems.
            std::vector<Processor> processors;
            int core_id;
        };

        std::map<int, Core> cores;
        int physical_id;
    };

    std::map<int, PhysicalCpu> physical_cpus;
};
}  // namespace affinity