#include <matchmaking/tournament/tournament_manager.hpp>

#include <util/logger/logger.hpp>

namespace fast_chess {

TournamentManager::TournamentManager(const options::Tournament& tournament_config,
                                     const std::vector<EngineConfiguration>& engine_configs, const stats_map& results)
    : engine_configs_(engine_configs), tournament_options_(tournament_config) {
    Logger::trace("Creating tournament...");

    // Set the seed for the random number generator
    Logger::trace("Seeding random number generator with seed: {}", tournament_options_.seed);
    util::random::mersenne_rand.seed(tournament_options_.seed);

    round_robin_ = std::make_unique<RoundRobin>(tournament_options_, engine_configs_, results);
}

void TournamentManager::start() {
    Logger::info("Starting tournament...");

    round_robin_->start();
}

}  // namespace fast_chess
