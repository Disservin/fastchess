#include "match.hpp"

namespace fast_chess {
Match::Match(const CMD::GameManagerOptions& game_config, const EngineConfiguration& engine1_config,
             const EngineConfiguration& engine2_config, const std::string& fen, int round) {
    engine1_.loadConfig(engine1_config);
    engine2_.loadConfig(engine2_config);

    engine1_.startEngine(engine1_config.cmd);
    engine2_.startEngine(engine2_config.cmd);

    data_.players.first.config = engine1_config;
    data_.players.second.config = engine2_config;

    data_.round = round;

    start(fen);
}

MatchData Match::get() const { return data_; }

void Match::start(const std::string& fen) {
    Chess::Board board(fen);

        engine1_.sendUciNewGame();
    engine2_.sendUciNewGame();
}

bool Match::playMove() { return false; }

}  // namespace fast_chess