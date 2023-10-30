#pragma once

#include <vector>
#include <array>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

namespace affinity {

/// @brief [physical id][2][processor id's]
/// @return
inline std::map<int, std::array<std::vector<int>, 2>> getPhysicalCores() noexcept(false) {
    std::ifstream cpuinfo("/proc/cpuinfo");

    // [physical id][core id][2][processor id's]
    std::map<int, std::map<int, std::vector<int>>> cpus;
    std::string line;

    int processorId = -1;
    int coreId      = -1;
    int physicalId  = -1;

    constexpr auto extract_value = [](const std::string& line) -> int {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            return std::stoi(line.substr(colonPos + 1));
        }

        return -1;
    };

    while (std::getline(cpuinfo, line)) {
        if (line.find("processor") != std::string::npos) {
            processorId = extract_value(line);
        } else if (line.find("core id") != std::string::npos) {
            coreId = extract_value(line);
        } else if (line.find("physical id") != std::string::npos) {
            physicalId = extract_value(line);
        }

        if (coreId != -1 && processorId != -1 && physicalId != -1) {
            cpus[physicalId][coreId].push_back(processorId);

            coreId      = -1;
            processorId = -1;
            physicalId  = -1;
        }
    }

    cpuinfo.close();

    // [physical id][2][processor id's]
    std::map<int, std::array<std::vector<int>, 2>> core_map;

    for (const auto& [physical_id, cores] : cpus) {
        std::vector<int> ht_1;
        std::vector<int> ht_2;

        for (const auto& [core_id, processor_ids] : cores) {
            for (size_t i = 0; i < processor_ids.size(); i++) {
                if (i % 2 == 0) {
                    ht_1.push_back(processor_ids[i]);
                } else {
                    ht_2.push_back(processor_ids[i]);
                }
            }
        }

        core_map[physical_id] = {ht_1, ht_2};
    }

    return core_map;
}

}  // namespace affinity