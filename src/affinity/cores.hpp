#pragma once

#include <iostream>
#include <stack>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include <affinity/cores_win.hpp>
#elif defined(__APPLE__)
#include <affinity/cores_mac.hpp>
#else
#include <affinity/cores_posix.hpp>
#endif

#include <third_party/chess.hpp>

namespace affinity {
class CoreHandler {
    enum Group {
        NONE = -1,
        HT_1,
        HT_2,
    };

    struct AffinityProcessor {
        int physical_id;
        Group group;
        std::size_t mask;

        AffinityProcessor(int physical_id, Group group, std::size_t mask)
            : physical_id(physical_id), group(group), mask(mask) {}
    };

   public:
    // Hyperthreads are split up into two groups: HT_1 and HT_2
    // So all elements in HT_1 are guaranteed to be on a different physical core.
    // Same goes for HT_2. This is done to avoid putting two processes on the same
    // physical core. When all cores in HT_1 are used, HT_2 is used.

    CoreHandler(bool use_affinity) {
        use_affinity_ = use_affinity;

        if (use_affinity_) {
            available_hardware_ = getPhysicalCores();
        }
    }

    /// @brief Get a core from the pool of available cores.
    ///
    /// @param threads 0 if no affinity is used, otherwise the number of threads.
    /// @return
    [[nodiscard]] AffinityProcessor consume(int threads) noexcept(false) {
        if (!use_affinity_) {
            return {0, Group::NONE, 0};
        }

        if (threads == 0) {
            return {0, Group::NONE, 0};
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        // physical_id is the id for the physical cpu, will be 0 for single cpu systems
        // cores is array of two vectors, each vector contains distinct processors which are
        // not on the same physical core. We distribute the workload first over all cores
        // in HT_1, then over all cores in HT_2.
        for (auto& [physical_id, bins] : available_hardware_) {
            std::size_t mask = 0ull;
            int count        = 0;

            for (size_t i = 0; i < bins.size(); i++) {
                while (!bins[i].empty()) {
                    const auto processor = bins[i].back();
                    bins[i].pop_back();

                    mask |= 1ull << processor;
                    count++;

                    if (count == threads) {
                        return {physical_id, static_cast<Group>(i), mask};
                    }

                    // we couldnt find a suitable mask, so we have to put the processor back
                    if (bins[i].empty()) {
                        for (int j = 0; j < count; j++) {
                            const int pos = chess::builtin::poplsb(mask);
                            bins[i].push_back(pos);
                        }

                        mask  = 0ull;
                        count = 0;
                    }
                }
            }
        }

        std::stringstream ss;
        ss << "Warning; No suiteable affinity mask found for " << threads << " threads\n."
           << "This is the case when you have specified more threads than\n"
           << "processors in a physical cpu. I.e. 32 System Threads, 4 Engine\n"
              "Threads only allow a concurrency of 8, with affinity turned on.\n"
           << "The program will continue to run, but will not use affinity.\n";

        std::cout << ss.str();

        return {0, Group::NONE, 0};
    }

    void put_back(AffinityProcessor core) noexcept {
        if (!use_affinity_) {
            return;
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        // extract the cores back
        while (core.mask) {
            const int pos = chess::builtin::poplsb(core.mask);
            available_hardware_[core.physical_id][core.group].push_back(pos);
        }
    }

   private:
    bool use_affinity_ = false;

    std::map<int, std::array<std::vector<int>, 2>> available_hardware_;
    std::mutex core_mutex_;
};

}  // namespace affinity