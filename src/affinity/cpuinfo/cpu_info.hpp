#pragma once

#include <map>
#include <vector>

namespace fast_chess {
namespace affinity {

/// @brief Contains information about all the cpu's in the system.
struct CpuInfo {
    struct PhysicalCpu {
        struct Core {
            struct Processor {
                Processor(int processor_id) : processor_id(processor_id) {}
                int processor_id;
            };

            Core() = default;

            Core(int core_id, const std::vector<Processor>& processor_ids)
                : processors(processor_ids), core_id(core_id) {}

            /// @brief Processor's in this core. Probably 2. Can be 1 on non-HT systems or something
            /// else on weird systems.
            std::vector<Processor> processors;
            int core_id;
        };

        std::map<int, Core> cores;
        int physical_id;
    };

    std::map<int, PhysicalCpu> physical_cpus;
};

}  // namespace affinity
}  // namespace fast_chess