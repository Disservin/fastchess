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

    const auto fill = [this, &engine_configs](std::size_t i, std::size_t j, std::size_t round_id) {
        // both players get the same opening
        const auto opening = fetchNextOpening();
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

                if (sprt_.isValid()) {
                    updateSprtStatus({first, second});
                }

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
                fill(i, j, k);
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
                          start_callback start, finished_callback finish, const Opening& opening,
                          std::size_t game_id) {
    auto match = Match(tournament_options_, opening);

    try {
        start();
        match.start(configs.first, configs.second);

        while (match.get().needs_restart) {
            match.start(configs.first, configs.second);
        }
    } catch (const std::exception& e) {
        Logger::error(e.what(), std::this_thread::get_id(), "fast-chess::RoundRobin::playGame");
        return;
    }

    if (atomic::stop) return;

    const auto match_data = match.get();
    const auto result     = extractStats(match_data);

    finish(result, match_data.reason);

    const auto pgn_builder = PgnBuilder(match_data, tournament_options_, game_id);

    // If the game was stopped, don't write the PGN
    if (match_data.termination != MatchTermination::INTERRUPT) {
        file_writer_.write(pgn_builder.get());
    }
}

Stats RoundRobin::extractStats(const MatchData& match_data) {
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