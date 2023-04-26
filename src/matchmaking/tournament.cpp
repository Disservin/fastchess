#include "tournament.hpp"

#include "../rand.hpp"
#include "../third_party/chess.hpp"

namespace fast_chess {
Tournament::Tournament(const CMD::GameManagerOptions& game_config) {
    const std::string filename =
        (game_config.pgn.file.empty() ? "fast-chess" : game_config.pgn.file) + ".pgn";

    file_writer_.open(filename);

    loadConfig(game_config);
}
void Tournament::loadConfig(const CMD::GameManagerOptions& game_config) {
    this->game_config_ = game_config;

    if (game_config_.games > 2)
        throw std::runtime_error("Error: Exceeded -game limit! Must be less than 2");

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

void Tournament::validateEngines(const std::vector<EngineConfiguration>& configs) {
    if (configs.size() < 2) {
        throw std::runtime_error("Error: Need at least two engines to start!");
    }

    for (size_t i = 0; i < configs.size(); i++) {
        for (size_t j = 0; j < i; j++) {
            if (configs[i].name == configs[j].name) {
                throw std::runtime_error("Error: Engine with the same name are not allowed!: " +
                                         configs[i].name);
            }
        }
    }

    engine_count_ = configs.size();
}

std::string Tournament::fetchNextFen() {
    static uint64_t fen_index_ = 0;

    if (opening_book_.size() == 0) {
        return Chess::STARTPOS;
    } else if (game_config_.opening.format == "pgn") {
        // TODO implementation
    } else if (game_config_.opening.format == "epd") {
        return opening_book_[(game_config_.opening.start + fen_index_++) % opening_book_.size()];
    }

    return Chess::STARTPOS;
}

void Tournament::start(const std::vector<EngineConfiguration>& engine_configs) {
    validateEngines(engine_configs);

    Logger::coutInfo("Starting tournament...");
    Logger::coutInfo("Finished tournament\nSaving results...");
}

void Tournament::stop() { pool_.kill(); }

}  // namespace fast_chess