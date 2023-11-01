#include <matchmaking/roundrobin.hpp>

#include <third_party/chess.hpp>

#include <matchmaking/output/output_factory.hpp>
#include <pgn_builder.hpp>
#include <util/logger.hpp>
#include <util/rand.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const cmd::TournamentOptions& game_config)
    : output_(getNewOutput(game_config.output)),
      tournament_options_(game_config),
      cores_(game_config.affinity) {
    auto filename = (game_config.pgn.file.empty() ? "fast-chess" : game_config.pgn.file);

    if (game_config.output == OutputType::FASTCHESS) {
        filename += ".pgn";
    }

    file_writer_.open(filename);

    setupEpdOpeningBook();
    setupPgnOpeningBook();

    // Resize the thread pool, but don't go over the number of threads the system has
    pool_.resize(std::min(static_cast<int>(std::thread::hardware_concurrency()),
                          tournament_options_.concurrency));

    // Initialize the SPRT test
    sprt_ = SPRT(tournament_options_.sprt.alpha, tournament_options_.sprt.beta,
                 tournament_options_.sprt.elo0, tournament_options_.sprt.elo1);
}

void RoundRobin::setupEpdOpeningBook() {
    // Set the seed for the random number generator
    random::mersenne_rand.seed(tournament_options_.seed);

    if (tournament_options_.opening.file.empty() ||
        tournament_options_.opening.format != FormatType::EPD) {
        return;
    }

    // Read the opening book from file
    std::ifstream openingFile;
    openingFile.open(tournament_options_.opening.file);

    std::string line;
    while (std::getline(openingFile, line)) {
        opening_book_epd_.emplace_back(line);
    }

    openingFile.close();

    if (opening_book_epd_.empty()) {
        throw std::runtime_error("No openings found in EPD file: " +
                                 tournament_options_.opening.file);
    }

    shuffle(opening_book_epd_);
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

    shuffle(opening_book_pgn_);
}

void RoundRobin::start(const std::vector<EngineConfiguration>& engine_configs) {
    Logger::log<Logger::Level::TRACE>("Starting round robin tournament...");

    create(engine_configs);

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
    auto match                = Match(tournament_options_, opening);
    const auto first_threads  = configs.first.threads();
    const auto second_threads = configs.second.threads();

    // thread count in both configs has to be the same for affinity to work,
    // otherwise we set it to 0 and affinity is disabled
    const auto max_threads_for_affinity = first_threads == second_threads ? first_threads : 0;
    const auto core                     = cores_.consume(max_threads_for_affinity);

    try {
        start();

        match.start(configs.first, configs.second, core.cpus);

        while (match.get().needs_restart) {
            match.start(configs.first, configs.second, core.cpus);
        }
    } catch (const std::exception& e) {
        Logger::log<Logger::Level::ERR>("Exception RoundRobin::playGame: " + std::string(e.what()));

        cores_.put_back(core);

        return;
    }

    cores_.put_back(core);

    if (atomic::stop) return;

    const auto match_data = match.get();
    const auto result     = extractStats(match_data);

    // If the game was stopped, don't write the PGN
    if (match_data.termination != MatchTermination::INTERRUPT) {
        file_writer_.write(PgnBuilder(match_data, tournament_options_, game_id).get());
    }

    finish(result, match_data.reason);
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

    const auto idx = tournament_options_.opening.start + opening_index++;

    if (tournament_options_.opening.format == FormatType::PGN) {
        return opening_book_pgn_[idx % opening_book_pgn_.size()];
    } else if (tournament_options_.opening.format == FormatType::EPD) {
        Opening opening;

        opening.fen = opening_book_epd_[idx % opening_book_epd_.size()];

        return opening;
    }

    return {chess::constants::STARTPOS, {}};
}

}  // namespace fast_chess
