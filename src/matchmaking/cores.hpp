#pragma once

#include <iostream>
#include <stack>
#include <mutex>

class CoreHandler {
   public:
    CoreHandler() {
        max_cores_ = std::thread::hardware_concurrency();
        available_cores_.resize(max_cores_);

        std::iota(available_cores_.begin(), available_cores_.end(), 0);
    }

    [[nodiscard]] uint32_t consume() {
        std::lock_guard<std::mutex> lock(core_mutex_);

        if (available_cores_.empty()) {
            throw std::runtime_error("No cores available.");
        }

        // first try to find a core with an even number
        const auto it = std::find_if(available_cores_.begin(), available_cores_.end(),
                                     [](uint32_t core) { return core % 2 == 0; });

        if (it != available_cores_.end()) {
            const uint32_t core = *it;
            available_cores_.erase(it);
            return core;
        }

        // fallback
        const uint32_t core = available_cores_.back();
        available_cores_.pop_back();
        return core;
    }

    void put_back(uint32_t core) {
        std::lock_guard<std::mutex> lock(core_mutex_);

        if (core >= max_cores_) {
            throw std::runtime_error("Core does not exist.");
        }

        available_cores_.emplace_back(core);
    }

   private:
    uint32_t max_cores_;
    std::vector<uint32_t> available_cores_;
    std::mutex core_mutex_;
};