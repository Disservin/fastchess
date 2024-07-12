#include <matchmaking/tournament/tournament_manager.hpp>

#include <util/logger/logger.hpp>

namespace fast_chess {

TournamentManager::TournamentManager(const stats_map& results) {
    Logger::trace("Creating tournament...");

    // Set the seed for the random number generator
    Logger::trace("Seeding random number generator with seed: {}", config::Tournament.get().seed);
    util::random::mersenne_rand.seed(config::Tournament.get().seed);

    round_robin_ = std::make_unique<RoundRobin>(results);
}

void TournamentManager::start() {
    Logger::info("Starting tournament...");

    round_robin_->start();
}

}  // namespace fast_chess
