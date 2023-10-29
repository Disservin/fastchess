#pragma once

#include <iostream>
#include <stack>
#include <mutex>

#ifdef _WIN32
#include <affinity/cores_win.hpp>
#elif defined(__APPLE__)
#include <affinity/cores_mac.hpp>
#else
#include <affinity/cores_posix.hpp>
#endif

namespace affinity {
class CoreHandler {
    enum Group {
        HT_1,
        HT_2,
    };

    struct Core {
        int physical_id;
        Group group;
        uint32_t mask;

        Core(int physical_id, Group group, int mask)
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
            available_cores_ = get_physical_cores();
        }
    }

    /// @brief Get a core from the pool of available cores.
    /// @return
    [[nodiscard]] Core consume() noexcept(false) {
        if (!use_affinity_) {
            return {0, Group::HT_1, -1};
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        for (auto& [physical_id, cores] : available_cores_) {
            for (size_t i = 0; i < cores.size(); i++) {
                if (!cores[i].empty()) {
                    const uint32_t core = cores[i].back();
                    cores[i].pop_back();

                    return {physical_id, static_cast<Group>(i), core};
                }
            }
        }

        assert(false);

        return {0, Group::HT_1, -1};
    }

    void put_back(Core core) noexcept {
        if (!use_affinity_) {
            return;
        }

        std::lock_guard<std::mutex> lock(core_mutex_);

        available_cores_[core.physical_id][core.group].push_back(core.mask);
    }

   private:
    bool use_affinity_ = false;

    std::map<int, std::array<std::vector<int>, 2>> available_cores_;
    std::mutex core_mutex_;
};

}  // namespace affinity