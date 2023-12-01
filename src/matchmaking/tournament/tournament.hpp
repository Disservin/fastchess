#pragma once

#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

#include <types/tournament_options.hpp>

namespace fast_chess {

/// @brief Manages the tournament, currenlty wraps round robin but can be extended to support
/// different tournament types
class Tournament {
   public:
    Tournament(const cmd::TournamentOptions &game_config,
               const std::vector<EngineConfiguration> &engine_configs);

    ~Tournament() {
        stop();
        saveJson();
    }

    void start();
    void stop() { round_robin_.stop(); }

    [[nodiscard]] RoundRobin *roundRobin() { return &round_robin_; }

   private:
    void saveJson() {
        nlohmann::ordered_json jsonfile = tournament_options_;
        jsonfile["engines"]             = engine_configs_;
        jsonfile["stats"]               = round_robin_.getResults();

        Logger::log<Logger::Level::TRACE>("Saving results...");

        std::ofstream file("config.json");
        file << std::setw(4) << jsonfile << std::endl;

        Logger::log<Logger::Level::INFO>("Saved results.");
    }

    void fixConfig();
    void validateEngines() const;

    std::vector<EngineConfiguration> engine_configs_;
    cmd::TournamentOptions tournament_options_;

    RoundRobin round_robin_;
};

}  // namespace fast_chess