#include <matchmaking/roundrobin.hpp>

#include <third_party/chess.hpp>

#include <logger.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <pgn_builder.hpp>
#include <rand.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const cmd::TournamentOptions& game_config)
    : output_(getNewOutput(game_config.output)), tournament_options_(game_config) {
    auto filename = (game_config.pgn.file.empty() ? "fast-chess" : game_config.pgn.file);

    if (game_config.output == OutputType::FASTCHESS) {
        filename += ".pgn";
    }

    file_writer_.open(filename);

    setupEpdOpeningBook();
    setupPgnOpeningBook();

    // Resize the thread pool
    pool_.resize(tournament_options_.concurrency);

    // Initialize the SPRT test
    sprt_ = SPRT(tournament_options_.sprt.alpha, tournament_options_.sprt.beta,
                 tournament_options_.sprt.elo0, tournament_options_.sprt.elo1);
}

void RoundRobin::setupEpdOpeningBook() {
    // Set the seed for the random number generator
    Random::mersenne_rand.seed(tournament_options_.seed);

    if (tournament_options_.opening.file.empty() ||
        tournament_options_.opening.format != FormatType::EPD) {
        return;
    }

    // Read the opening book from file
    std::ifstream openingFile;
    std::string line;
    openingFile.open(tournament_options_.opening.file);

    while (std::getline(openingFile, line)) {
        opening_book_epd_.emplace_back(line);
    }

    openingFile.close();

    if (opening_book_epd_.empty()) {
        throw std::runtime_error("No openings found in EPD file: " +
                                 tournament_options_.opening.file);
    }

    if (tournament_options_.opening.order == OrderType::RANDOM) {
        // Fisher-Yates / Knuth shuffle
        for (std::size_t i = 0; i <= opening_book_epd_.size() - 2; i++) {
            std::size_t j = i + (Random::mersenne_rand() % (opening_book_epd_.size() - i));
            std::swap(opening_book_epd_[i], opening_book_epd_[j]);
        }
    }
}

void RoundRobin::setupPgnOpeningBook() {
    // Read the opening book from file
    if (tournament_options_.opening.file.empty() ||
        tournament_options_.opening.format != FormatType::PGN) {
        return;
    }

    const PgnReader pgn_reader = PgnReader(tournament_options_.opening.file);
    opening_book_pgn_          = pgn_reader.getPgns();

    if (opening_book_pgn_.empty()) {
        throw std::runtime_error("No openings found in PGN file: " +
                                 tournament_options_.opening.file);
    }

    if (tournament_options_.opening.order == OrderType::RANDOM) {
        // Fisher-Yates / Knuth shuffle
        for (std::size_t i = 0; i <= opening_book_pgn_.size() - 2; i++) {
            std::size_t j = i + (Random::mersenne_rand() % (opening_book_pgn_.size() - i));
            std::swap(opening_book_pgn_[i], opening_book_pgn_[j]);
        }
    }
}

void RoundRobin::start(const std::vector<EngineConfiguration>& engine_configs) {
    Logger::debug("Starting round robin tournament...");

    create(engine_configs);

    // Wait for games to finish
    while (match_count_ < total_ && !atomic::stop) {
    }
}

void RoundRobin::create(const std::vector<EngineConfiguration>& engine_configs) {
    total_ = (engine_configs.size() * (engine_configs.size() - 1) / 2) *
             tournament_options_.rounds * tournament_options_.games;

    for (std::size_t i = 0; i < engine_configs.size(); i++) {
        for (std::size_t j = i + 1; j < engine_configs.size(); j++) {
            for (int k = 0; k < tournament_options_.rounds; k++) {
                // both players get the same opening
                const auto opening = fetchNextOpening();
                const auto first   = engine_configs[i];
                const auto second  = engine_configs[j];

                auto configs = std::pair{engine_configs[i], engine_configs[j]};

                for (int i = 0; i < tournament_options_.games; i++) {
                    const auto round_id = k * tournament_options_.games + (i + 1);

                    const auto start = [this, round_id, configs]() {
                        output_->startGame(configs.first.name, configs.second.name, round_id,
                                           total_);
                    };

                    const auto finish = [this, engine_configs, i, j, round_id, configs, first,
                                         second](const Stats& stats, const std::string& reason) {
                        match_count_++;

                        output_->endGame(stats, configs.first.name, configs.second.name, reason,
                                         round_id);

                        if (!tournament_options_.report_penta) {
                            result_.updateStats(configs.first.name, configs.second.name, stats);

                            output_->printInterval(sprt_, result_.getStats(first.name, second.name),
                                                   first.name, second.name, match_count_);
                        }

                        if (sprt_.isValid()) {
                            updateSprtStatus({first, second});
                        }
                    };

                    pool_.enqueue(&RoundRobin::playGame, this, configs, opening, round_id, start,
                                  finish);

                    // swap players
                    std::swap(configs.first, configs.second);
                }
            }
        }
    }
}

void RoundRobin::updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs) {
    const Stats stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
    const double llr  = sprt_.getLLR(stats.wins, stats.draws, stats.losses);

    if (sprt_.getResult(llr) != SPRT_CONTINUE || match_count_ == total_) {
        atomic::stop = true;

        Logger::cout("SPRT test finished: " + sprt_.getBounds() + " " + sprt_.getElo());
        output_->printElo(stats, engine_configs[0].name, engine_configs[1].name, match_count_);
        output_->endTournament();

        stop();
    }
}

void RoundRobin::playGame(const std::pair<EngineConfiguration, EngineConfiguration>& configs,
                          const Opening& opening, int round_id, start_callback start,
                          finished_callback finish) {
    if (atomic::stop) return;

    auto match = Match(tournament_options_, opening, round_id);

    try {
        start();
        match.start(configs.first, configs.second);
    } catch (const std::exception& e) {
        Logger::error(e.what(), std::this_thread::get_id(), "fast-chess::RoundRobin::playGame");
        return;
    }

    const auto match_data = match.get();
    const auto result     = updateStats(match_data);

    finish(result, match_data.reason);

    const auto pgn_builder = PgnBuilder(match_data, tournament_options_);

    // If the game was stopped, don't write the PGN
    if (match_data.termination != MatchTermination::INTERRUPT) {
        file_writer_.write(pgn_builder.get());
    }
}

Stats RoundRobin::updateStats(const MatchData& match_data) {
    Stats stats;

    if (match_data.players.first.result == chess::GameResult::WIN) {
        stats.wins++;
    } else if (match_data.players.first.result == chess::GameResult::LOSE) {
        stats.losses++;
    } else {
        stats.draws++;
    }

    return stats;
}

Opening RoundRobin::fetchNextOpening() {
    static uint64_t opening_index = 0;

    if (tournament_options_.opening.format == FormatType::PGN) {
        return opening_book_pgn_[(tournament_options_.opening.start + opening_index++) %
                                 opening_book_pgn_.size()];
    } else if (tournament_options_.opening.format == FormatType::EPD) {
        Opening opening;

        opening.fen = opening_book_epd_[(tournament_options_.opening.start + opening_index++) %
                                        opening_book_epd_.size()];

        return opening;
    }

    Logger::cout("Unknown opening format: " + int(tournament_options_.opening.format));

    return {chess::STARTPOS, {}};
}

}  // namespace fast_chess