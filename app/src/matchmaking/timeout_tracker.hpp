#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace fastchess {

class TimeoutTracker {
   public:
    // Increments the timeout counter for the player
    void timeout(const std::string &player) { timeouts_[player]++; }

    // Resets the timeout counter for the player
    void reset(const std::string &player) { timeouts_[player] = 0; }

    // Returns the number of timeouts for the player
    [[nodiscard]] size_t get(const std::string &player) const { return timeouts_.at(player); }

    [[nodiscard]] auto begin() const { return timeouts_.begin(); }
    [[nodiscard]] auto end() const { return timeouts_.end(); }

    // Resets the timeout counter for all players
    void resetAll() { timeouts_.clear(); }

   private:
    // string, how often the player has timed out
    std::unordered_map<std::string, size_t> timeouts_;
};

}  // namespace fastchess
