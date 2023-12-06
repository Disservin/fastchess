#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

#include <chess.hpp>

#include <matchmaking/output/output_factory.hpp>
#include <pgn/pgn_builder.hpp>
#include <util/logger.hpp>
#include <util/rand.hpp>
#include <util/scope_guard.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const options::Tournament& tournament_config)
    : ITournament(tournament_config) {
    // Initialize the SPRT test
    sprt_ = SPRT(tournament_options_.sprt.alpha, tournament_options_.sprt.beta,
                 tournament_options_.sprt.elo0, tournament_options_.sprt.elo1);
}

void RoundRobin::start(const std::vector<EngineConfiguration>& engine_configs) {
    ITournament::start(engine_configs);

    // Wait for games to finish
    while (match_count_ < total_ && !atomic::stop) {
    }
}

void RoundRobin::create(const std::vector<EngineConfiguration>& engine_configs) {
    total_ = (engine_configs.size() * (engine_configs.size() - 1) / 2) *
             tournament_options_.rounds * tournament_options_.games;

    const auto create_match = [this, &engine_configs](std::size_t i, std::size_t j,
                                                      std::size_t round_id) {
        // both players get the same opening
        const auto opening = book_.fetch();
        const auto first   = engine_configs[i];
        const auto second  = engine_configs[j];

        auto configs = std::pair{engine_configs[i], engine_configs[j]};

        for (int g = 0; g < tournament_options_.games; g++) {
            const std::size_t game_id = round_id * tournament_options_.games + (g + 1);

            // callback functions, do not capture by reference
            const auto start = [this, configs, game_id]() {
                output_->startGame(configs, game_id, total_);
            };

            // callback functions, do not capture by reference
            const auto finish = [this, configs, first, second, game_id, round_id](
                                    const Stats& stats, const std::string& reason) {
                output_->endGame(configs, stats, reason, game_id);

                bool complete_pair = false;

                if (tournament_options_.report_penta) {
                    complete_pair = result_.updatePairStats(configs, first.name, stats, round_id);
                } else {
                    result_.updateStats(configs, stats);
                }

                // Only print the interval if the pair is complete or we are not tracking
                // penta stats.
                if (!tournament_options_.report_penta || complete_pair) {
                    const auto updated_stats = result_.getStats(first.name, second.name);

                    output_->printInterval(sprt_, updated_stats, first.name, second.name,
                                           match_count_ + 1);
                }

                updateSprtStatus({first, second});

                match_count_++;
            };

            pool_.enqueue(&RoundRobin::playGame, this, configs, start, finish, opening, round_id);

            // swap players
            std::swap(configs.first, configs.second);
        }
    };

    for (std::size_t i = 0; i < engine_configs.size(); i++) {
        for (std::size_t j = i + 1; j < engine_configs.size(); j++) {
            for (int k = 0; k < tournament_options_.rounds; k++) {
                create_match(i, j, k);
            }
        }
    }
}

void RoundRobin::updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs) {
    if (!sprt_.isValid()) return;

    const auto stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
    const auto llr   = sprt_.getLLR(stats.wins, stats.draws, stats.losses);

    if (sprt_.getResult(llr) != SPRT_CONTINUE || match_count_ == total_) {
        atomic::stop = true;

        Logger::log<Logger::Level::INFO>("SPRT test finished: " + sprt_.getBounds() + " " +
                                         sprt_.getElo());

        output_->printElo(stats, engine_configs[0].name, engine_configs[1].name, match_count_);
        output_->endTournament();

        stop();
    }
}

void RoundRobin::playGame(const std::pair<EngineConfiguration, EngineConfiguration>& configs,
                          start_callback start, finished_callback finish, const Opening& opening,
                          std::size_t game_id) {
    const auto core = ScopeGuard(cores_->consume());

    auto engine_one = ScopeGuard(engine_cache_.getEntry(configs.first.name, configs.first));
    auto engine_two = ScopeGuard(engine_cache_.getEntry(configs.second.name, configs.second));

    start();

    auto match = Match(tournament_options_, opening);

    try {
        match.start(engine_one.get().get(), engine_two.get().get(), core.get().cpus);

        while (match.get().needs_restart) {
            match.start(engine_one.get().get(), engine_two.get().get(), core.get().cpus);
        }

    } catch (const std::exception& e) {
        Logger::log<Logger::Level::ERR>("Exception RoundRobin::playGame: " + std::string(e.what()));

        return;
    }

    if (atomic::stop) return;

    const auto match_data = match.get();

    // If the game was stopped, don't write the PGN
    if (match_data.termination != MatchTermination::INTERRUPT) {
        file_writer_.write(PgnBuilder(match_data, tournament_options_, game_id).get());
    }

    finish({match_data}, match_data.reason);
}

}  // namespace fast_chess
