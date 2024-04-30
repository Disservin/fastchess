#pragma once

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <affinity/cpuinfo/cpu_info.hpp>

namespace fast_chess::affinity::cpu_info {

inline CpuInfo getCpuInfo() {
    std::ifstream cpuinfo("/proc/cpuinfo");

    std::string line;

    int processorId = -1;
    int coreId      = -1;
    int physicalId  = -1;

    bool procIdFound     = false;
    bool coreIdFound     = false;
    bool physicalIdFound = false;

    auto unset_id = [&]() {
        coreId      = -1;
        processorId = -1;
        physicalId  = -1;

        procIdFound     = false;
        coreIdFound     = false;
        physicalIdFound = false;
    };

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

            // Already found process ID before, but haven't pushed
            if (procIdFound) {
                cpu_info.physical_cpus[physicalIdFound ? physicalId : 0]
                        .cores[coreIdFound ? coreIdFound : 0]
                        .processors.emplace_back(processorId);

                unset_id();
            }

            procIdFound = true;
            processorId = extract_value(line);

        } else if (line.find("core id") != std::string::npos) {
            coreId      = extract_value(line);
            coreIdFound = true;
        } else if (line.find("physical id") != std::string::npos) {
            physicalId      = extract_value(line);
            physicalIdFound = true;
        }

        if (coreId != -1 && processorId != -1 && physicalId != -1) {
            cpu_info.physical_cpus[physicalId].cores[coreId].processors.emplace_back(processorId);

            unset_id();
        }
    }
  
    if (procIdFound) {
        cpu_info.physical_cpus[physicalIdFound ? physicalId : 0]
                .cores[coreIdFound ? coreIdFound : 0]
                .processors.emplace_back(processorId);

        unset_id();
    }

    return cpu_info;
}

}  // namespace fast_chess
