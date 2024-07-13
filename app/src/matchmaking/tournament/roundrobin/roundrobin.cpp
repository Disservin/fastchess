#include <matchmaking/tournament/roundrobin/roundrobin.hpp>

#include <chess.hpp>

#include <matchmaking/output/output_factory.hpp>
#include <pgn/pgn_builder.hpp>
#include <util/logger/logger.hpp>
#include <util/rand.hpp>
#include <util/scope_guard.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const stats_map& results) : BaseTournament(results) {
    // Initialize the SPRT test
    sprt_ = SPRT(config::TournamentConfig.get().sprt.alpha, config::TournamentConfig.get().sprt.beta,
                 config::TournamentConfig.get().sprt.elo0, config::TournamentConfig.get().sprt.elo1,
                 config::TournamentConfig.get().sprt.model, config::TournamentConfig.get().sprt.enabled);
}

void RoundRobin::start() {
    Logger::trace("Starting round robin tournament...");

    BaseTournament::start();

    // If autosave is enabled, save the results every save_interval games
    const auto save_interval = config::TournamentConfig.get().autosaveinterval;
    // Account for the initial matchcount
    auto save_iter = initial_matchcount_ + save_interval;

    // Wait for games to finish
    while (match_count_ < total_ && !atomic::stop) {
        if (save_interval > 0 && match_count_ >= save_iter) {
            saveJson();
            save_iter += save_interval;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void RoundRobin::create() {
    Logger::trace("Creating matches...");

    total_ = (config::EngineConfigs.get().size() * (config::EngineConfigs.get().size() - 1) / 2) *
             config::TournamentConfig.get().rounds * config::TournamentConfig.get().games;

    const auto create_match = [this](std::size_t i, std::size_t j, std::size_t round_id, int g,
                                     std::optional<std::size_t> opening_id) {
        assert(g < 2);

        constexpr static auto normalize_stm_configs = [](const pair_config& configs, const chess::Color stm) {
            // swap players if the opening is for black, to ensure that
            // reporting the result is always white vs black
            if (stm == chess::Color::BLACK) {
                return std::pair{configs.second, configs.first};
            }

            return configs;
        };

        constexpr static auto normalize_stats = [](const Stats& stats, const chess::Color stm) {
            // swap stats if the opening is for black, to ensure that
            // reporting the result is always white vs black
            if (stm == chess::Color::BLACK) {
                return ~stats;
            }

            return stats;
        };

        const auto opening = (*book_)[opening_id];

        const std::size_t game_id = round_id * config::TournamentConfig.get().games + (g + 1);
        const auto stm            = opening.stm;
        const auto first          = config::EngineConfigs.get()[i];
        const auto second         = config::EngineConfigs.get()[j];
        auto configs              = std::pair{config::EngineConfigs.get()[i], config::EngineConfigs.get()[j]};

        if (g == 1) {
            std::swap(configs.first, configs.second);
        }

        // callback functions, do not capture by reference
        const auto start = [this, configs, game_id, stm]() {
            output_->startGame(normalize_stm_configs(configs, stm), game_id, total_);
        };

        // callback functions, do not capture by reference
        const auto finish = [this, configs, first, second, game_id, round_id, stm](
                                const Stats& stats, const std::string& reason, const engines& engines) {
            const auto normalized_configs = normalize_stm_configs(configs, stm);
            const auto normalized_stats   = normalize_stats(stats, stm);

            output_->endGame(normalized_configs, normalized_stats, reason, game_id);

            bool report = true;

            if (config::TournamentConfig.get().report_penta)
                report = result_.updatePairStats(configs, first.name, stats, round_id);
            else
                result_.updateStats(configs, stats);

            // round_id and match_count_ starts 0 so we add 1
            const auto ratinginterval_index =
                config::TournamentConfig.get().report_penta ? round_id + 1 : match_count_ + 1;
            const auto scoreinterval_index = match_count_ + 1;
            const auto updated_stats       = result_.getStats(first.name, second.name);

            // print score result based on scoreinterval if output format is cutechess
            if ((scoreinterval_index % config::TournamentConfig.get().scoreinterval == 0) ||
                match_count_ + 1 == total_) {
                output_->printResult(updated_stats, first.name, second.name);
            }

            // Only print the interval if the pair is complete or we are not tracking
            // penta stats.
            if ((report && ratinginterval_index % config::TournamentConfig.get().ratinginterval == 0) ||
                match_count_ + 1 == total_) {
                output_->printInterval(sprt_, updated_stats, first.name, second.name, engines,
                                       config::TournamentConfig.get().opening.file);
            }

            updateSprtStatus({first, second}, engines);

            match_count_++;
        };

        playGame(configs, start, finish, opening, round_id);

        if (g == 1) {
            std::swap(configs.first, configs.second);
        }
    };

    for (std::size_t i = 0; i < config::EngineConfigs.get().size(); i++) {
        for (std::size_t j = i + 1; j < config::EngineConfigs.get().size(); j++) {
            int offset = initial_matchcount_ / config::TournamentConfig.get().games;
            for (int k = offset; k < config::TournamentConfig.get().rounds; k++) {
                // both players get the same opening
                const auto opening = book_->fetchId();

                for (int g = 0; g < config::TournamentConfig.get().games; g++) {
                    pool_.enqueue(create_match, i, j, k, g, opening);
                }
            }
        }
    }
}

void RoundRobin::updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs, const engines& engines) {
    if (!sprt_.isEnabled()) return;

    const auto stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
    const auto llr   = sprt_.getLLR(stats, config::TournamentConfig.get().report_penta);

    if (sprt_.getResult(llr) != SPRT_CONTINUE || match_count_ == total_) {
        atomic::stop = true;

        Logger::info("SPRT test finished: {} {}", sprt_.getBounds(), sprt_.getElo());
        output_->printResult(stats, engine_configs[0].name, engine_configs[1].name);
        output_->printInterval(sprt_, stats, engine_configs[0].name, engine_configs[1].name, engines,
                               config::TournamentConfig.get().opening.file);
        output_->endTournament();

        stop();
    }
}

}  // namespace fast_chess
