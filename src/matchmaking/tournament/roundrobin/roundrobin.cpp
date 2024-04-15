#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

#include <chess.hpp>

#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/tournament/roundrobin/iterator.hpp>
#include <pgn/pgn_builder.hpp>
#include <util/logger/logger.hpp>
#include <util/rand.hpp>
#include <util/scope_guard.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const options::Tournament& tournament_config,
                       const std::vector<EngineConfiguration>& engine_configs)
    : BaseTournament(tournament_config, engine_configs) {
    // Initialize the SPRT test
    sprt_ = SPRT(tournament_options_.sprt.alpha, tournament_options_.sprt.beta,
                 tournament_options_.sprt.elo0, tournament_options_.sprt.elo1,
                 tournament_options_.sprt.logistic_bounds);
}

void RoundRobin::start() {
    BaseTournament::start();

    // Wait for games to finish
    while (match_count_ < total_ && !atomic::stop) {
    }
}

void RoundRobin::create() {
    total_ = (engine_configs_.size() * (engine_configs_.size() - 1) / 2) *
             tournament_options_.rounds * tournament_options_.games;

    pool_.consumeIterator(&RoundRobin::playGame,
                          RoundRobinIterator(tournament_options_, engine_configs_, book_, *output_),
                          this);
}

void RoundRobin::updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs) {
    if (!sprt_.isValid()) return;

    const auto stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
    const auto llr   = tournament_options_.report_penta
                           ? sprt_.getLLR(stats.penta_WW, stats.penta_WD, stats.penta_WL,
                                          stats.penta_DD, stats.penta_LD, stats.penta_LL)
                           : sprt_.getLLR(stats.wins, stats.draws, stats.losses);

    if (sprt_.getResult(llr) != SPRT_CONTINUE || match_count_ == total_) {
        atomic::stop = true;

        Logger::log<Logger::Level::INFO>("SPRT test finished: " + sprt_.getBounds() + " " +
                                         sprt_.getElo());

        output_->printElo(stats, engine_configs[0].name, engine_configs[1].name);
        output_->endTournament();

        stop();
    }
}

}  // namespace fast_chess
