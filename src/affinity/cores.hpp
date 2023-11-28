#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <stack>

#ifdef _WIN32
#include <affinity/cores_win.hpp>
#elif defined(__APPLE__)
#include <affinity/cores_mac.hpp>
#else
#include <affinity/cores_posix.hpp>
#endif

#include <chess.hpp>

namespace affinity {
class CoreHandler {
    struct AffinityProcessor {
        std::vector<int> cpus;
        AffinityProcessor(const std::vector<int>& cpus) : cpus(cpus) {}
    };

   public:
    // Hyperthreads are split up into two groups: HT_1 and HT_2
    // So all elements in HT_1 are guaranteed to be on a different physical core.
    // Same goes for HT_2. This is done to avoid putting two processes on the same
    // physical core. When all cores in HT_1 are used, HT_2 is used.

    /// @brief
    /// @param use_affinity
    /// @param tpe threads per engine
    CoreHandler(bool use_affinity, int tpe) {
        use_affinity_ = use_affinity;

        if (tpe > 1) {
            use_affinity_ = false;
        }

        if (use_affinity_) {
            available_hardware_ = getPhysicalCores();

            setupCores();
        }
    }

    /// @brief Setup the cores for the affinity, later entries from the core pool will be just
    /// picked up.
    void setupCores() {
        std::lock_guard<std::mutex> lock(core_mutex_);

        // physical_id is the id for the physical cpu, will be 0 for single cpu systems
        // cores is array of two vectors, each vector contains distinct processors which are
        // not on the same physical core. We distribute the workload first over all cores
        // in HT_1, then over all cores in HT_2.

        /// @todo: fix logic for multiple threads and multiple concurrencies

        for (std::size_t i = 0; i < 2; i++) {
            for (auto& [physical_id, bins] : available_hardware_) {
                while (!bins[i].empty()) {
                    const auto processor = bins[i].back();
                    bins[i].pop_back();

                    cores_.emplace_back(std::vector{processor});
                }
            }
        }
    }

    /// @brief Get a core from the pool of available cores.
    ///
    /// @return
    [[nodiscard]] AffinityProcessor consume() {
        if (!use_affinity_) {
            return {{}};
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        if (cores_.empty()) {
            throw std::runtime_error("No cores available");
        }

        const auto core = cores_.back();
        cores_.pop_back();

        return core;
    }

    void put_back(const AffinityProcessor& core) noexcept {
        if (!use_affinity_) {
            return;
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        cores_.push_back(core);
    }

   private:
    bool use_affinity_ = false;

    std::vector<AffinityProcessor> cores_;

    std::map<int, std::array<std::vector<int>, 2>> available_hardware_;
    std::mutex core_mutex_;
};

}  // namespace affinity