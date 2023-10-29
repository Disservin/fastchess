#pragma once

#include <vector>
#include <array>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fast_chess {
inline std::array<std::vector<int>, 2> get_physical_cores() {
    std::ifstream cpuinfo("/proc/cpuinfo");

    std::unordered_map<int, std::vector<int>> physical_cpus;
    std::string line;

    int coreId      = -1;
    int processorId = -1;

    while (std::getline(cpuinfo, line)) {
        if (line.find("processor") != std::string::npos) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                processorId = std::stoi(line.substr(colonPos + 1));
            }
        }
        // physical core
        else if (line.find("core id") != std::string::npos) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                coreId = std::stoi(line.substr(colonPos + 1));
            }
        }

        if (coreId != -1 && processorId != -1) {
            physical_cpus[coreId].push_back(processorId);
            coreId      = -1;
            processorId = -1;
        }
    }

    cpuinfo.close();

    std::vector<int> ht_1;
    std::vector<int> ht_2;

    ht_1.reserve(physical_cpus.size());
    ht_2.reserve(physical_cpus.size());

    for (const auto& [coreId, processorIds] : physical_cpus) {
        for (size_t i = 0; i < processorIds.size(); i++) {
            if (i % 2 == 0) {
                ht_1.push_back(processorIds[i]);
            } else {
                ht_2.push_back(processorIds[i]);
            }
        }
    }

    return {ht_1, ht_2};
}
}  // namespace fast_chess