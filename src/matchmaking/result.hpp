#pragma once

#include <mutex>
#include <string>
#include <unordered_map>


#include <matchmaking/types/stats.hpp>

namespace fast_chess {

using stats_map = std::unordered_map<std::string, std::unordered_map<std::string, Stats>>;

class Result {
   public:
    /// @brief Updates the stats of engine1 vs engine2
    /// @param engine1
    /// @param engine2
    /// @param stats
    void updateStats(const std::string& engine1, const std::string& engine2, const Stats& stats) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_[engine1][engine2] += stats;
    }

    /// @brief Stats of engine1 vs engine2, adjusted with the perspective
    /// @param engine1
    /// @param engine2
    /// @return
    [[nodiscard]] Stats getStats(const std::string& engine1, const std::string& engine2) {
        std::lock_guard<std::mutex> lock(results_mutex_);

        const auto stats1 = results_[engine1][engine2];
        const auto stats2 = results_[engine2][engine1];

        // we need to collect the results of engine1 vs engine2 and engine2 vs engine1
        // and combine them so that engine2's wins are engine1's losses and vice versa
        return stats1 + ~stats2;
    }

    /// @brief
    /// @return
    [[nodiscard]] stats_map getResults() {
        std::lock_guard<std::mutex> lock(results_mutex_);
        return results_;
    }

    /// @brief
    /// @param results
    void setResults(const stats_map& results) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_ = results;
    }

   private:
    /// @brief tracks the engine results
    stats_map results_;
    std::mutex results_mutex_;
};

}  // namespace fast_chess
