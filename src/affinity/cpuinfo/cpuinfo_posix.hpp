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
#include <util/logger/logger.hpp>

namespace affinity {

inline CpuInfo getCpuInfo() {
    fast_chess::Logger::log<Logger::Level::TRACE>("Fetching CPU info...");

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

    fast_chess::Logger::log<Logger::Level::TRACE>("Finished fetching CPU info...");

    return cpu_info;
}

}  // namespace affinity