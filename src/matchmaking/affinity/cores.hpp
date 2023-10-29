#pragma once

#include <iostream>
#include <stack>
#include <mutex>

#ifdef _WIN32
#include <matchmaking/affinity/cores_win.hpp>
#elif defined(__APPLE__)
#include <matchmaking/affinity/cores_mac.hpp>
#else
#include <matchmaking/affinity/cores_posix.hpp>
#endif

namespace affinity {
class CoreHandler {
   public:
    // Hyperthreads are split up into two groups: HT_1 and HT_2
    // So all elements in HT_1 are guaranteed to be on a different physical core.
    // Same goes for HT_2. This is done to avoid putting two processes on the same
    // physical core. When all cores in HT_1 are used, HT_2 is used.
    enum CoreType {
        HT_1,
        HT_2,
    };

    CoreHandler() { available_cores_ = get_physical_cores(); }

    /// @brief Get a core from the pool of available cores.
    ///
    /// @return
    [[nodiscard]] std::pair<CoreType, int> consume() noexcept(false) {
        std::lock_guard<std::mutex> lock(core_mutex_);

        // Prefer HT_1 over HT_2, can also be vice versa.
        if (!available_cores_[HT_1].empty()) {
            const uint32_t core = available_cores_[HT_1].back();
            available_cores_[HT_1].pop_back();

            return {CoreType::HT_1, core};
        }

        if (available_cores_[HT_2].empty()) {
            throw std::runtime_error("No cores available.");
        }

        const uint32_t core = available_cores_[HT_2].back();
        available_cores_[HT_2].pop_back();

        return {CoreType::HT_2, core};
    }

    void put_back(std::pair<CoreType, int> core) noexcept {
        std::lock_guard<std::mutex> lock(core_mutex_);

        available_cores_[core.first].push_back(core.second);
    }

   private:
    std::array<std::vector<int>, 2> available_cores_;
    std::mutex core_mutex_;
};

}  // namespace affinity