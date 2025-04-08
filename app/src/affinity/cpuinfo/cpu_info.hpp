#pragma once

#include <map>
#include <vector>

namespace fastchess::affinity::cpu_info {

// Represents the complete CPU topology of the system
struct CpuInfo {
    struct PhysicalPackage {
        struct PhysicalCore {
            struct LogicalProcessor {
                LogicalProcessor(int logical_id) : logical_id(logical_id) {}
                int logical_id;
            };

            PhysicalCore() = default;

            PhysicalCore(int core_id, const std::vector<LogicalProcessor>& logical_processors)
                : logical_processors(logical_processors), core_id(core_id) {}

            // Logical processors in this physical core. Typically 2 with SMT enabled,
            // 1 on non-SMT systems, or more on specialized architectures.
            std::vector<LogicalProcessor> logical_processors;
            int core_id;
        };

        std::map<int, PhysicalCore> cores;
        int socket_id;
    };

    std::map<int, PhysicalPackage> packages;
};

}  // namespace fastchess::affinity::cpu_info