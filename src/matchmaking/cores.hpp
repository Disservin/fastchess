#pragma once

#include <iostream>
#include <vector>
#include <mutex>

class CoreHandler {
   public:
    CoreHandler() {
        max_cores = getMaxCores();
        for (uint32_t i = 0; i < max_cores; i++) {
            available_cores.push_back(i);
        }
    }

    uint32_t consume() {
        std::lock_guard<std::mutex> lock(core_mutex);

        if (available_cores.empty()) {
            throw std::runtime_error("No cores available.");
        }

        uint32_t core = available_cores.back();
        available_cores.pop_back();
        in_use_cores.push_back(core);
        return core;
    }

    void release(int core) {
        std::lock_guard<std::mutex> lock(core_mutex);

        auto it = std::find(in_use_cores.begin(), in_use_cores.end(), core);

        if (it != in_use_cores.end()) {
            in_use_cores.erase(it);
            available_cores.push_back(core);
        } else {
            throw std::runtime_error("Core " + std::to_string(core) +
                                     " is not in use and cannot be released.");
        }
    }

   private:
    /// @brief Get the maximum number of cores available on the system.
    /// //
    /// https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
    /// @return
    uint32_t getMaxCores() const {
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
#error "TODO"
#else
#error "Cannot detect number of cores on this system"
#endif
    }

    uint32_t max_cores;
    std::vector<uint32_t> available_cores;
    std::vector<uint32_t> in_use_cores;
    std::mutex core_mutex;
};