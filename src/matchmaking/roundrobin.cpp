#include "roundrobin.hpp"

#include "../logger.hpp"
#include "../rand.hpp"
#include "../third_party/chess.hpp"

namespace fast_chess {

RoundRobin::RoundRobin(const CMD::GameManagerOptions& game_config) {
    this->game_config_ = game_config;
    this->output_ = std::make_unique<Fastchess>();

    const std::string filename =
        (game_config.pgn.file.empty() ? "fast-chess" : game_config.pgn.file) + ".pgn";

    file_writer_.open(filename);

    // Set the seed for the random number generator
    Random::mersenne_rand.seed(game_config_.seed);

    // Read the opening book from file
    if (!game_config_.opening.file.empty()) {
        std::ifstream openingFile;
        std::string line;
        openingFile.open(game_config_.opening.file);

        while (std::getline(openingFile, line)) {
            opening_book_.emplace_back(line);
        }

        openingFile.close();

        if (game_config_.opening.order == "random") {
            // Fisher-Yates / Knuth shuffle
            for (std::size_t i = 0; i <= opening_book_.size() - 2; i++) {
                std::size_t j = i + (Random::mersenne_rand() % (opening_book_.size() - i));
                std::swap(opening_book_[i], opening_book_[j]);
            }
        }
    }

    // Initialize the thread pool
    pool_.resize(game_config_.concurrency);

    // Initialize the SPRT test
    sprt_ = SPRT(game_config_.sprt.alpha, game_config_.sprt.beta, game_config_.sprt.elo0,
                 game_config_.sprt.elo1);
}

std::string RoundRobin::fetchNextFen() {
    static uint64_t fen_index_ = 0;

    if (opening_book_.size() == 0) {
        return Chess::STARTPOS;
    } else if (game_config_.opening.format == "pgn") {
        // TODO implementation
        throw std::runtime_error("PGN opening book not implemented yet");
    } else if (game_config_.opening.format == "epd") {
        return opening_book_[(game_config_.opening.start + fen_index_++) % opening_book_.size()];
    }

    return Chess::STARTPOS;
}

void RoundRobin::start(const std::vector<EngineConfiguration>& engine_configs) {
    std::vector<std::future<void>> results;

    create(engine_configs, results);

    while (!results.empty()) {
        Logger::cout("Waiting for " + std::to_string(results.size()) + " results to finish...");
        results.back().get();
        results.pop_back();
    }
}

void RoundRobin::create(const std::vector<EngineConfiguration>& engine_configs,
                        std::vector<std::future<void>>& results) {
    for (std::size_t i = 0; i < engine_configs.size(); i++) {
        for (std::size_t j = i + 1; j < engine_configs.size(); j++) {
            for (int k = 0; k < game_config_.rounds; k++) {
                Logger::cout("Added " + engine_configs[i].name + " vs " + engine_configs[j].name +
                             " to the queue");
                results.emplace_back(pool_.enqueue(&RoundRobin::createPairings, this,
                                                   engine_configs[i], engine_configs[j]));
            }
        }
    }
}

void RoundRobin::createPairings(const EngineConfiguration& player1,
                                const EngineConfiguration& player2) {
    std::pair<EngineConfiguration, EngineConfiguration> configs = {player1, player2};

    auto random_bit = Random::mersenne_rand() & 1;

    if (random_bit && game_config_.output == "cutechess") {
        std::swap(configs.first, configs.second);
    }

    Stats stats;
    auto fen = fetchNextFen();
    for (int i = 0; i < game_config_.games; i++) {
        output_->startGame();

        auto [success, result] = playGame(configs, fen, i);
        if (success) {
            stats += result;
            total++;

            if (!game_config_.report_penta) {
                result_.updateStats(configs.first.name, configs.second.name, result);
                output_->printInterval(result_.getStats(player1.name, player2.name), player1.name,
                                       player2.name, total);
            }

            std::swap(configs.first, configs.second);
        }

        output_->endGame();
    }

    stats.penta_WW += stats.wins == 2;
    stats.penta_WD += stats.wins == 1 && stats.draws == 1;
    stats.penta_WL += stats.wins == 1 && stats.losses == 1;
    stats.penta_DD += stats.draws == 2;
    stats.penta_LD += stats.losses == 1 && stats.draws == 1;
    stats.penta_LL += stats.losses == 2;

    if (game_config_.report_penta) {
        result_.updateStats(configs.first.name, configs.second.name, stats);
    }

    output_->printInterval(result_.getStats(player1.name, player2.name), player1.name, player2.name,
                           total);
}

std::pair<bool, Stats> RoundRobin::playGame(
    const std::pair<EngineConfiguration, EngineConfiguration>& configs, const std::string& fen,
    int round_id) {
    Match match = Match(game_config_, configs.first, configs.second, fen, round_id);

    MatchData match_data = match.get();

    auto stats = updateStats(match_data);

    return {true, stats};
}

Stats RoundRobin::updateStats(const MatchData& match_data) {
    Stats stats;

    if (match_data.players.first.result == Chess::GameResult::WIN) {
        stats.wins++;
    } else if (match_data.players.first.result == Chess::GameResult::LOSE) {
        stats.losses++;
    } else {
        stats.draws++;
    }

    return stats;
}

}  // namespace fast_chess