#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace fastchess {

struct Tracker {
    std::size_t timeouts    = 0;
    std::size_t disconnects = 0;
};

class PlayerTracker {
   public:
    [[nodiscard]] auto get(const std::string &player) const { return count_.at(player); }
    [[nodiscard]] auto &get(const std::string &player) { return count_.at(player); }

    [[nodiscard]] auto begin() const { return count_.begin(); }
    [[nodiscard]] auto end() const { return count_.end(); }

    // Resets the counter for all players
    void resetAll() { count_.clear(); }

   private:
    std::unordered_map<std::string, Tracker> count_;
};

}  // namespace fastchess
