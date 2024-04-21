#pragma once

#include <matchmaking/tournament/roundrobin/roundrobin.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess {

/// @brief Manages the tournament, currently wraps round robin but can be extended to support
/// different tournament types
class TournamentManager {
   public:
    TournamentManager(const options::Tournament &game_config,
                      const std::vector<EngineConfiguration> &engine_configs);

    ~TournamentManager() {
        Logger::log<Logger::Level::TRACE>("Destroying tournament manager...");
        stop();
        saveJson();
    }

    void start();
    void stop() { round_robin_->stop(); }

    [[nodiscard]] RoundRobin *roundRobin() { return round_robin_.get(); }

   private:
    void saveJson() {
        nlohmann::ordered_json jsonfile = tournament_options_;
        jsonfile["engines"]             = engine_configs_;
        jsonfile["stats"]               = round_robin_->getResults();

        Logger::log<Logger::Level::TRACE>("Saving results...");

        std::ofstream file("config.json");
        file << std::setw(4) << jsonfile << std::endl;

        Logger::log<Logger::Level::INFO>("Saved results.");
    }

    options::Tournament fixConfig(options::Tournament config);
    void validateEngines() const;

    std::vector<EngineConfiguration> engine_configs_;
    options::Tournament tournament_options_;

    std::unique_ptr<RoundRobin> round_robin_;
};

}  // namespace fast_chess