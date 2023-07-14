#include <matchmaking/roundrobin.hpp>

#include <third_party/chess.hpp>

#include <logger.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <pgn_builder.hpp>
#include <rand.hpp>

namespace fast_chess {

RoundRobin::RoundRobin(const cmd::GameManagerOptions& game_config) {
    this->game_config_ = game_config;

    this->output_ = getNewOutput(game_config_.output);

    const std::string filename =
        (game_config.pgn.file.empty() ? "fast-chess" : game_config.pgn.file) + ".pgn";

    file_writer_.open(filename);

    setupEpdOpeningBook();
    setupPgnOpeningBook();

    // Resize the thread pool
    pool_.resize(game_config_.concurrency);

    // Initialize the SPRT test
    sprt_ = SPRT(game_config_.sprt.alpha, game_config_.sprt.beta, game_config_.sprt.elo0,
                 game_config_.sprt.elo1);
}

void RoundRobin::setupEpdOpeningBook() {
    // Set the seed for the random number generator
    Random::mersenne_rand.seed(game_config_.seed);

    if (game_config_.opening.file.empty() || game_config_.opening.format != cmd::FormatType::EPD) {
        return;
    }

    // Read the opening book from file
    std::ifstream openingFile;
    std::string line;
    openingFile.open(game_config_.opening.file);

    while (std::getline(openingFile, line)) {
        opening_book_epd.emplace_back(line);
    }

    openingFile.close();

    if (opening_book_epd.empty()) {
        throw std::runtime_error("No openings found in EPD file: " + game_config_.opening.file);
    }

    if (game_config_.opening.order == cmd::OrderType::RANDOM) {
        // Fisher-Yates / Knuth shuffle
        for (std::size_t i = 0; i <= opening_book_epd.size() - 2; i++) {
            std::size_t j = i + (Random::mersenne_rand() % (opening_book_epd.size() - i));
            std::swap(opening_book_epd[i], opening_book_epd[j]);
        }
    }
}

void RoundRobin::setupPgnOpeningBook() {
    // Read the opening book from file
    if (game_config_.opening.file.empty() || game_config_.opening.format != cmd::FormatType::PGN) {
        return;
    }

    PgnReader pgn_reader = PgnReader(game_config_.opening.file);
    opening_book_pgn = pgn_reader.getPgns();

    if (opening_book_pgn.empty()) {
        throw std::runtime_error("No openings found in PGN file: " + game_config_.opening.file);
    }

    if (game_config_.opening.order == cmd::OrderType::RANDOM) {
        // Fisher-Yates / Knuth shuffle
        for (std::size_t i = 0; i <= opening_book_pgn.size() - 2; i++) {
            std::size_t j = i + (Random::mersenne_rand() % (opening_book_pgn.size() - i));
            std::swap(opening_book_pgn[i], opening_book_pgn[j]);
        }
    }
}

void RoundRobin::start(const std::vector<EngineConfiguration>& engine_configs) {
    std::vector<std::future<void>> results;

    create(engine_configs, results);

    if (sprt(engine_configs)) return;

    while (!results.empty()) {
        Logger::cout("Waiting for " + std::to_string(results.size()) + " results to finish...");
        results.back().get();
        results.pop_back();
    }
}

void RoundRobin::create(const std::vector<EngineConfiguration>& engine_configs,
                        std::vector<std::future<void>>& results) {
    total_ = (engine_configs.size() * (engine_configs.size() - 1) / 2) * game_config_.rounds *
             game_config_.games;

    for (std::size_t i = 0; i < engine_configs.size(); i++) {
        for (std::size_t j = i + 1; j < engine_configs.size(); j++) {
            for (int k = 0; k < game_config_.rounds; k++) {
                results.emplace_back(pool_.enqueue(&RoundRobin::createPairings, this,
                                                   engine_configs[i], engine_configs[j], k));
            }
        }
    }
}

bool RoundRobin::sprt(const std::vector<EngineConfiguration>& engine_configs) {
    Logger::debug("Trying to enter SPRT test...");
    if (engine_configs.size() != 2 || !sprt_.isValid()) return false;

    Logger::cout("SPRT test started: " + sprt_.getBounds() + " " + sprt_.getElo());

    while (match_count_ < total_ || !atomic::stop) {
    }

    Logger::cout("SPRT test skipped");

    return true;
}

void RoundRobin::updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs) {
    Stats stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
    const double llr = sprt_.getLLR(stats.wins, stats.draws, stats.losses);

    if (sprt_.getResult(llr) != SPRT_CONTINUE || match_count_ == total_) {
        atomic::stop = true;

        Logger::cout("SPRT test finished: " + sprt_.getBounds() + " " + sprt_.getElo());
        output_->printElo(stats, engine_configs[0].name, engine_configs[1].name, match_count_);
        output_->endTournament();

        stop();
    }
}

void RoundRobin::createPairings(const EngineConfiguration& player1,
                                const EngineConfiguration& player2, int current) {
    std::pair<EngineConfiguration, EngineConfiguration> configs = {player1, player2};

    if (Random::boolean() && game_config_.output == OutputType::CUTECHESS) {
        std::swap(configs.first, configs.second);
    }

    const auto opening = fetchNextOpening();

    Stats stats;
    for (int i = 0; i < game_config_.games; i++) {
        const auto idx = current * game_config_.games + i;

        output_->startGame(configs.first.name, configs.second.name, idx, game_config_.rounds * 2);
        const auto [success, result, reason] = playGame(configs, opening, idx);
        output_->endGame(result, configs.first.name, configs.second.name, reason, idx);

        // If the game failed to start, try again
        if (!success && game_config_.recover) {
            i--;
            continue;
        }

        // Invert the result of the other player, so that stats are always from the perspective of
        // the first player.
        if (player1.name != configs.first.name) {
            stats += ~result;
        } else {
            stats += result;
        }

        match_count_++;

        if (!game_config_.report_penta) {
            result_.updateStats(configs.first.name, configs.second.name, result);
            output_->printInterval(sprt_, result_.getStats(player1.name, player2.name),
                                   player1.name, player2.name, match_count_);
        }

        std::swap(configs.first, configs.second);
    }

    // track penta stats
    if (game_config_.report_penta) {
        stats.penta_WW += stats.wins == 2;
        stats.penta_WD += stats.wins == 1 && stats.draws == 1;
        stats.penta_WL += stats.wins == 1 && stats.losses == 1;
        stats.penta_DD += stats.draws == 2;
        stats.penta_LD += stats.losses == 1 && stats.draws == 1;
        stats.penta_LL += stats.losses == 2;

        result_.updateStats(configs.first.name, configs.second.name, stats);
        output_->printInterval(sprt_, result_.getStats(player1.name, player2.name), player1.name,
                               player2.name, match_count_);
    }

    if (sprt_.isValid()) {
        updateSprtStatus({player1, player2});
    }
}

std::tuple<bool, Stats, std::string> RoundRobin::playGame(
    const std::pair<EngineConfiguration, EngineConfiguration>& configs, const Opening& opening,
    int round_id) {
    Match match = Match(game_config_, configs.first, configs.second, opening, round_id);

    try {
        match.start();
    } catch (const std::exception& e) {
        Logger::error(e.what(), std::this_thread::get_id(), "fast-chess::RoundRobin::playGame");
        throw e;
    }

    MatchData match_data = match.get();

    PgnBuilder pgn_builder = PgnBuilder(match_data, game_config_);

    if (match_data.termination != "stop") {
        file_writer_.write(pgn_builder.get());
    }

    return {true, updateStats(match_data), match_data.internal_reason};
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

    if (game_config_.opening.format == cmd::FormatType::PGN) {
        return opening_book_pgn[(game_config_.opening.start + opening_index++) %
                                opening_book_pgn.size()];
    } else if (game_config_.opening.format == cmd::FormatType::EPD) {
        Opening opening;

        opening.fen = opening_book_epd[(game_config_.opening.start + opening_index++) %
                                       opening_book_epd.size()];

        // epd books dont have any moves, so we just return the epd string
        opening.moves = {};

        return opening;
    }

    return {chess::STARTPOS, {}};
}

}  // namespace fast_chess