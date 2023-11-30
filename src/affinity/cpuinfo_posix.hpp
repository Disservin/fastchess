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

#include <affinity/cpu_info.hpp>

namespace affinity {

inline CpuInfo getCpuInfo() {
    std::ifstream cpuinfo("/proc/cpuinfo");

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

    CpuInfo cpu_info;

    while (std::getline(cpuinfo, line)) {
        if (line.find("processor") != std::string::npos) {
            processorId = extract_value(line);
        } else if (line.find("core id") != std::string::npos) {
            coreId = extract_value(line);
        } else if (line.find("physical id") != std::string::npos) {
            physicalId = extract_value(line);
        }

        if (coreId != -1 && processorId != -1 && physicalId != -1) {
            cpu_info.physical_cpus[physicalId].cores[coreId].processors.emplace_back(processorId);

            coreId      = -1;
            processorId = -1;
            physicalId  = -1;
        }
    }

    return cpu_info;
}

}  // namespace affinity