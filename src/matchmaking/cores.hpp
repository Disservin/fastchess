#pragma once

#include <iostream>
#include <stack>
#include <mutex>

class CoreHandler {
   public:
    CoreHandler() {
        max_cores_ = 1;  // getMaxCores();
        for (uint32_t i = 0; i < max_cores_; i++) {
            available_cores_.push(i);
        }
    }

    [[nodiscard]] uint32_t consume() {
        std::lock_guard<std::mutex> lock(core_mutex_);

        if (available_cores_.empty()) {
            throw std::runtime_error("No cores available.");
        }

        uint32_t core = available_cores_.top();
        available_cores_.pop();
        return core;
    }

    void put_back(uint32_t core) {
        std::lock_guard<std::mutex> lock(core_mutex_);

        if (core >= max_cores_) {
            throw std::runtime_error("Core does not exist.");
        }

        available_cores_.push(core);
    }

   private:
    /// @brief Get the maximum number of cores available on the system.
    /// //
    /// https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
    /// @return
    [[nodiscard]] uint32_t getMaxCores() const {
#include <stdint.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
        uint32_t num_cores   = 0;
        size_t num_cores_len = sizeof(num_cores);

        sysctlbyname("hw.physicalcpu", &num_cores, &num_cores_len, 0, 0);

        return num_cores;
#elif defined(__linux__)
#include <unistd.h>
        uint32_t lcores = 0, tsibs = 0;

        char buff[32];
        char path[64];

        for (lcores = 0;; lcores++) {
            FILE *cpu;

            snprintf(path, sizeof(path),
                     "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", lcores);

            cpu = fopen(path, "r");
            if (!cpu) break;

            while (fscanf(cpu, "%[0-9]", buff)) {
                tsibs++;
                if (fgetc(cpu) != ',') break;
            }

            fclose(cpu);
        }

        return lcores / (tsibs / lcores);
#elif defined(_WIN32)
#include <windows.h>
        DWORD byte_length = 0;

        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &byte_length);

        std::unique_ptr<char[]> buffer(new char[byte_length]);

        if (!GetLogicalProcessorInformationEx(
                RelationProcessorCore, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get()),
                &byte_length)) {
            std::cerr << "GetLogicalProcessorInformationEx failed." << std::endl;
            return -1;
        }

        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ptr =
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get());

        DWORD offset       = 0;
        uint32_t num_cores = 0;

        while (offset < byte_length) {
            if (ptr->Relationship == RelationProcessorCore) {
                num_cores++;
            }

            offset += ptr->Size;

            ptr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX(buffer.get() + offset);
        }

        return num_cores;
#else
#error "Cannot detect number of cores on this system"
#endif
    }

    uint32_t max_cores_;
    std::stack<uint32_t> available_cores_;
    std::mutex core_mutex_;
};