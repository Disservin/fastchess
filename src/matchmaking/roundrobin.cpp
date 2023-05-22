#include <matchmaking/roundrobin.hpp>

#include <third_party/chess.hpp>

#include <logger.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <pgn_builder.hpp>
#include <rand.hpp>
#include "roundrobin.hpp"


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

    // Read the opening book from file
    if (game_config_.opening.file.empty()) {
        return;
    }

    std::ifstream openingFile;
    std::string line;
    openingFile.open(game_config_.opening.file);

    while (std::getline(openingFile, line)) {
        opening_book_.emplace_back(line);
    }

    openingFile.close();

    if (game_config_.opening.order == cmd::OrderType::RANDOM) {
        // Fisher-Yates / Knuth shuffle
        for (std::size_t i = 0; i <= opening_book_.size() - 2; i++) {
            std::size_t j = i + (Random::mersenne_rand() % (opening_book_.size() - i));
            std::swap(opening_book_[i], opening_book_[j]);
        }
    }

    if (opening_book_.empty()) {
        throw std::runtime_error("No openings found in EPD file: " + game_config_.opening.file);
    }
}

void RoundRobin::setupPgnOpeningBook() {
    // Read the opening book from file
    if (game_config_.opening.file.empty()) {
        return;
    }

    PgnReader pgn_reader = PgnReader(game_config_.opening.file);
    pgn_opening_book_ = pgn_reader.getPgns();

    if (game_config_.opening.order == cmd::OrderType::RANDOM) {
        // Fisher-Yates / Knuth shuffle
        for (std::size_t i = 0; i <= pgn_opening_book_.size() - 2; i++) {
            std::size_t j = i + (Random::mersenne_rand() % (pgn_opening_book_.size() - i));
            std::swap(pgn_opening_book_[i], pgn_opening_book_[j]);
        }
    }

    if (pgn_opening_book_.empty()) {
        throw std::runtime_error("No openings found in PGN file: " + game_config_.opening.file);
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
    if (engine_configs.size() != 2 || !sprt_.isValid()) return false;

    Logger::cout("SPRT test started: " + sprt_.getBounds() + " " + sprt_.getElo());

    while (engine_configs.size() == 2 && sprt_.isValid() && match_count_ < total_ &&
           !Atomic::stop) {
        Stats stats = result_.getStats(engine_configs[0].name, engine_configs[1].name);
        const double llr = sprt_.getLLR(stats.wins, stats.draws, stats.losses);

        if (sprt_.getResult(llr) != SPRT_CONTINUE) {
            Atomic::stop = true;

            Logger::cout("SPRT test finished: " + sprt_.getBounds() + " " + sprt_.getElo());
            output_->printElo(stats, engine_configs[0].name, engine_configs[1].name, match_count_);
            output_->endTournament();
            return true;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }

    Logger::cout("SPRT test skipped", engine_configs.size() == 2, sprt_.isValid(), match_count_,
                 total_, !Atomic::stop);

    return true;
}

void RoundRobin::createPairings(const EngineConfiguration& player1,
                                const EngineConfiguration& player2, int current) {
    std::pair<EngineConfiguration, EngineConfiguration> configs = {player1, player2};

    if (Random::boolean() && game_config_.output == OutputType::CUTECHESS) {
        std::swap(configs.first, configs.second);
    }

    Stats stats;
    auto opening = fetchNextOpening();
    for (int i = 0; i < game_config_.games; i++) {
        auto idx = current * game_config_.games + i;

        output_->startGame(configs.first.name, configs.second.name, idx, game_config_.rounds * 2);
        auto [success, result, reason] = playGame(configs, opening, idx);
        output_->endGame(result, configs.first.name, configs.second.name, reason, idx);

        // If the game failed to start, try again
        if (!success && game_config_.recover) {
            i--;
            continue;
        }

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

    stats.penta_WW += stats.wins == 2;
    stats.penta_WD += stats.wins == 1 && stats.draws == 1;
    stats.penta_WL += stats.wins == 1 && stats.losses == 1;
    stats.penta_DD += stats.draws == 2;
    stats.penta_LD += stats.losses == 1 && stats.draws == 1;
    stats.penta_LL += stats.losses == 2;

    if (game_config_.report_penta) {
        result_.updateStats(configs.first.name, configs.second.name, stats);
        output_->printInterval(sprt_, result_.getStats(player1.name, player2.name), player1.name,
                               player2.name, match_count_);
    }
}

std::tuple<bool, Stats, std::string> RoundRobin::playGame(
    const std::pair<EngineConfiguration, EngineConfiguration>& configs, const Opening& opening,
    int round_id) {
    Match match = Match(game_config_, configs.first, configs.second, opening, round_id);
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
        return pgn_opening_book_[(game_config_.opening.start + opening_index++) %
                                 pgn_opening_book_.size()];
    } else if (game_config_.opening.format == cmd::FormatType::EPD) {
        return {
            opening_book_[(game_config_.opening.start + opening_index++) % opening_book_.size()],
            {}};
    }

    return {chess::STARTPOS, {}};
}

}  // namespace fast_chess