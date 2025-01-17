#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fastchess {

struct Tracker {
    std::atomic<std::size_t> timeouts    = 0;
    std::atomic<std::size_t> disconnects = 0;
};

class PlayerTracker {
   public:
    void report_timeout(const std::string &player) {
        std::lock_guard<std::mutex> lock(mutex_);
        count_[player].timeouts++;
    }

    void report_disconnect(const std::string &player) {
        std::lock_guard<std::mutex> lock(mutex_);
        count_[player].disconnects++;
    }

    [[nodiscard]] auto begin() const { return count_.begin(); }
    [[nodiscard]] auto end() const { return count_.end(); }

    // Resets the counter for all players
    void resetAll() { count_.clear(); }

   private:
    std::unordered_map<std::string, Tracker> count_;
    std::mutex mutex_;
};

}  // namespace fastchess
