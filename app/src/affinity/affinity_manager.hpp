#pragma once

#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stack>

#include <chess.hpp>

#ifdef _WIN32
#    include <affinity/cpuinfo/cpuinfo_win.hpp>
#elif defined(__APPLE__)
#    include <affinity/cpuinfo/cpuinfo_mac.hpp>
#else
#    include <affinity/cpuinfo/cpuinfo_posix.hpp>
#endif

#include <affinity/cpuinfo/cpu_info.hpp>
#include <util/logger/logger.hpp>
#include <util/scope_guard.hpp>

namespace fastchess {

namespace affinity {
class AffinityManager {
    enum Group {
        NONE = -1,
        HT_1,
        HT_2,
    };

    class AffinityProcessor : public util::ScopeEntry {
       public:
        AffinityProcessor(const std::vector<int>& cpus) : util::ScopeEntry(true), cpus(cpus) {}

        std::vector<int> cpus;

        friend class AffinityManager;
    };

   public:
    // Hyperthreads are split up into two groups: HT_1 and HT_2
    // So all elements in HT_1 are guaranteed to be on a different physical core.
    // Same goes for HT_2. This is done to avoid putting two processes on the same
    // physical core. When all cores in HT_1 are used, HT_2 is used.

    // Construct a new Affinity Manager object
    AffinityManager(bool use_affinity, int tpe) {
        use_affinity_ = use_affinity;

        if (tpe > 1) {
            use_affinity_ = false;
        }

        if (use_affinity_) {
            setupCores(cpu_info::getCpuInfo());
            Logger::trace("Using affinity");
        }
    }

    // Get a core from the pool of available cores.
    [[nodiscard]] AffinityProcessor& consume() {
        if (!use_affinity_) {
            return null_core_;
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        if (cores_[HT_1].empty() && cores_[HT_2].empty()) {
            Logger::trace("No cores available");

            throw std::runtime_error("No cores available");
        }

        // find first available core
        for (const auto grp : {HT_1, HT_2}) {
            for (auto& core : cores_[grp]) {
                if (core.available_) {
                    core.available_ = false;
                    return core;
                }
            }
        }

        Logger::trace("No cores available, all are in use");

        throw std::runtime_error("No cores available");
    }

   private:
    // Setup the cores for the affinity, later entries from the core pool will be just
    // picked up.
    void setupCores(const cpu_info::CpuInfo& cpu_info) {
        Logger::trace("Setting up cores");
        std::lock_guard<std::mutex> lock(core_mutex_);

        // @TODO: fix logic for multiple threads and multiple concurrencies

        for (const auto& physical_cpu : cpu_info.physical_cpus) {
            for (const auto& core : physical_cpu.second.cores) {
                int idx = 0;
                for (const auto& processor : core.second.processors) {
                    Group group = idx % 2 == 0 ? HT_1 : HT_2;

                    // this looks wrong??
                    cores_[group].emplace_back(std::vector{processor.processor_id});

                    idx++;
                }
            }
        }
    }

    std::array<std::deque<AffinityProcessor>, 2> cores_;
    std::mutex core_mutex_;

    // This is a dummy core which is returned when affinity is disabled.
    AffinityProcessor null_core_ = {{}};

    bool use_affinity_ = false;
};

}  // namespace affinity
}  // namespace fastchess
