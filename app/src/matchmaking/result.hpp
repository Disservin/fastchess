#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <matchmaking/stats.hpp>
#include <types/engine_config.hpp>

namespace fast_chess {

struct PlayerPairKey {
    std::string first;
    std::string second;

    std::size_t hash;

    PlayerPairKey(std::string f, std::string s) {
        first  = std::move(f);
        second = std::move(s);

        hash = Hash{}(first, second);
    }

    [[nodiscard]] bool operator==(const PlayerPairKey& other) const noexcept {
        return (first == other.first && second == other.second);
    }

    [[nodiscard]] bool operator!=(const PlayerPairKey& other) const noexcept { return !(*this == other); }

    struct Hash {
        [[nodiscard]] std::size_t operator()(const PlayerPairKey& pair) const noexcept { return pair.hash; }

        [[nodiscard]] std::size_t operator()(const std::string& first, const std::string& second) const noexcept {
            return std::hash<std::string>{}(first + second);
        }
    };
};

using stats_map = std::unordered_map<PlayerPairKey, Stats, PlayerPairKey::Hash>;

inline void to_json(nlohmann::ordered_json& j, const stats_map& map) {
    nlohmann::ordered_json jtmp;

    for (const auto& item : map) {
        nlohmann::ordered_json obj = item.second;

        jtmp[item.first.first + " vs " + item.first.second] = obj;
    }

    j = jtmp;
}

inline void from_json(const nlohmann::ordered_json& j, stats_map& map) {
    for (const auto& item : j) {
        const auto key   = item.at("key").get<std::string>();
        const auto value = item.at("value").get<Stats>();

        map[PlayerPairKey(key.substr(0, key.find(" vs ")), key.substr(key.find(" vs ") + 4))] = value;
    }
}

class StatsMap {
   public:
    Stats& operator[](const GamePair<EngineConfiguration, EngineConfiguration>& configs) noexcept {
        return results_[PlayerPairKey(configs.white.name, configs.black.name)];
    }

    const Stats& operator[](const GamePair<EngineConfiguration, EngineConfiguration>& configs) const noexcept {
        return results_.at(PlayerPairKey(configs.white.name, configs.black.name));
    }

    Stats& operator[](const std::pair<std::string, std::string>& players) noexcept {
        return results_[PlayerPairKey(players.first, players.second)];
    }

    const Stats& operator[](const std::pair<std::string, std::string>& players) const noexcept {
        return results_.at(PlayerPairKey(players.first, players.second));
    }

    friend class Result;

   private:
    stats_map results_;
};

class Result {
   public:
    // Updates the stats of engine1 vs engine2
    void updateStats(const GamePair<EngineConfiguration, EngineConfiguration>& configs, const Stats& stats) noexcept {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_[configs] += stats;
    }

    // Update the stats in pair batches to keep track of pentanomial stats.
    [[nodiscard]] bool updatePairStats(const GamePair<EngineConfiguration, EngineConfiguration>& configs,
                                       const Stats& stats, uint64_t round_id) noexcept {
        std::lock_guard<std::mutex> lock(game_pair_cache_mutex_);

        const auto is_first_game = game_pair_cache_.find(round_id) == game_pair_cache_.end();
        auto& lookup             = game_pair_cache_[round_id];

        if (is_first_game) {
            // invert because other player
            lookup = ~stats;
            return false;
        }

        lookup += stats;

        lookup.penta_WW += lookup.wins == 2;
        lookup.penta_WD += lookup.wins == 1 && lookup.draws == 1;
        lookup.penta_WL += lookup.wins == 1 && lookup.losses == 1;
        lookup.penta_DD += lookup.draws == 2;
        lookup.penta_LD += lookup.losses == 1 && lookup.draws == 1;
        lookup.penta_LL += lookup.losses == 2;

        updateStats(configs, lookup);

        game_pair_cache_.erase(round_id);

        return true;
    }

    // Stats of engine1 vs engine2 + engine2 vs engine1, adjusted with the perspective
    [[nodiscard]] Stats getStats(const std::string& engine1, const std::string& engine2) noexcept {
        std::lock_guard<std::mutex> lock(results_mutex_);

        const auto stats1 = results_[{engine1, engine2}];
        const auto stats2 = results_[{engine2, engine1}];

        return stats1 + ~stats2;
    }

    [[nodiscard]] stats_map getResults() noexcept {
        std::lock_guard<std::mutex> lock(results_mutex_);
        return results_.results_;
    }

    void setResults(const stats_map& results) noexcept {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_.results_ = results;
    }

   private:
    // tracks the engine results
    StatsMap results_;
    std::mutex results_mutex_;

    std::unordered_map<uint64_t, Stats> game_pair_cache_;
    std::mutex game_pair_cache_mutex_;
};

}  // namespace fast_chess
