#include "match.hpp"

namespace fast_chess {

using namespace Chess;

Match::Match(const CMD::GameManagerOptions& game_config, const EngineConfiguration& engine1_config,
             const EngineConfiguration& engine2_config, const std::string& fen, int round) {
    game_config_ = game_config;

    Participant player_1;
    Participant player_2;

    player_1.engine.loadConfig(engine1_config);
    player_2.engine.loadConfig(engine2_config);

    player_1.engine.startEngine(engine1_config.cmd);
    player_2.engine.startEngine(engine2_config.cmd);

    data_.players.first.config = engine1_config;
    data_.players.second.config = engine2_config;

    data_.round = round;

    start_fen_ = fen;

    start(player_1, player_2, fen);
}

MatchData Match::get() const { return data_; }

void Match::start(Participant& engine1, Participant& engine2, const std::string& fen) {
    Board board(fen);

    engine1.engine.sendUciNewGame();
    engine2.engine.sendUciNewGame();

    // copy time control which will be updated later
    engine1.time_control = engine1.info.config.limit.tc;
    engine2.time_control = engine2.info.config.limit.tc;

    engine1.info.color = board.sideToMove();
    engine2.info.color = ~board.sideToMove();

    data_.fen = fen;

    data_.start_time = Logger::getDateTime();
    data_.date = Logger::getDateTime("%Y-%m-%d");

    try {
        while (true) {
            if (!playMove(engine1, engine2)) break;

            if (!playMove(engine2, engine1)) break;
        }
    } catch (const std::exception& e) {
        if (game_config_.recover)
            data_.needs_restart = true;
        else
            throw e;
    }
}

bool Match::playMove(Participant& us, Participant& opponent) {
    const auto gameover = board_.isGameOver();

    if (gameover == GameResult::DRAW) {
        us.info.result = GameResult::DRAW;
        opponent.info.result = GameResult::DRAW;

        return false;
    } else if (gameover == GameResult::LOSE) {
        us.info.result = GameResult::LOSE;
        opponent.info.result = GameResult::WIN;

        return false;
    }

    if (!us.engine.isResponsive()) {
        data_.termination = "timeout";
        us.info.result = GameResult::LOSE;
        opponent.info.result = GameResult::WIN;

        return false;
    }

    us.engine.writeEngine(us.engine.buildPositionInput(played_moves_, start_fen_));
    us.engine.writeEngine(
        us.engine.buildGoInput(board_.sideToMove(), us.time_control, opponent.time_control));

    const auto t0 = std::chrono::high_resolution_clock::now();
    auto output = us.engine.readEngine("bestmove", us.getTimeoutThreshold());
    const auto t1 = std::chrono::high_resolution_clock::now();

    const auto elapsed_millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (!us.updateTc(elapsed_millis)) {
        data_.termination = "timeout";

        us.info.result = GameResult::LOSE;
        opponent.info.result = GameResult::WIN;

        Logger::coutInfo("Warning: Engine", us.info.config.name, "loses on time #");

        return false;
    }

    const auto best_move = us.engine.bestmove();
    const auto score_type = us.engine.lastScoreType();
    const auto score = us.engine.lastScore();
    const auto move = board_.uciToMove(best_move);

    played_moves_.push_back(best_move);
}

}  // namespace fast_chess