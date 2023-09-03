#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include <types/stats.hpp>
#include <types/engine_config.hpp>

namespace fast_chess {

using stats_map   = std::unordered_map<std::string, std::unordered_map<std::string, Stats>>;
using pair_config = std::pair<fast_chess::EngineConfiguration, fast_chess::EngineConfiguration>;

class Result {
   public:
    /// @brief Updates the stats of engine1 vs engine2
    /// @param engine1
    /// @param engine2
    /// @param stats
    void updateStats(const pair_config& configs, const Stats& stats) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_[configs.first.name][configs.second.name] += stats;
    }

    /// @brief Update the stats in pair batches to keep track of pentanomial stats.
    /// @param first
    /// @param engine1
    /// @param engine2
    /// @param stats
    /// @param round_id
    /// @return
    [[nodiscard]] bool updatePairStats(const pair_config& configs, const std::string& first,
                                       const Stats& stats, uint64_t round_id) {
        std::lock_guard<std::mutex> lock(game_pair_cache_mutex_);

        const auto adjusted = first == configs.first.name ? stats : ~stats;

        const auto is_first_game = game_pair_cache_.find(round_id) == game_pair_cache_.end();

        auto& lookup = game_pair_cache_[round_id];

        if (is_first_game) {
            lookup = adjusted;
            return false;
        } else {
            lookup += adjusted;

            lookup.penta_WW += lookup.wins == 2;
            lookup.penta_WD += lookup.wins == 1 && lookup.draws == 1;
            lookup.penta_WL += lookup.wins == 1 && lookup.losses == 1;
            lookup.penta_DD += lookup.draws == 2;
            lookup.penta_LD += lookup.losses == 1 && lookup.draws == 1;
            lookup.penta_LL += lookup.losses == 2;

            updateStats(configs, first == configs.first.name ? lookup : ~lookup);

            game_pair_cache_.erase(round_id);

            return true;
        }
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

    std::unordered_map<uint64_t, Stats> game_pair_cache_;
    std::mutex game_pair_cache_mutex_;
};

}  // namespace fast_chess
