#include "match.hpp"

#include "../helper.hpp"

namespace fast_chess {

using namespace std::literals;
namespace chrono = std::chrono;
using clock = chrono::high_resolution_clock;

using namespace Chess;

Match::Match(const CMD::GameManagerOptions& game_config, const EngineConfiguration& engine1_config,
             const EngineConfiguration& engine2_config, const std::string& fen, int round) {
    game_config_ = game_config;

    Participant player_1;
    Participant player_2;

    player_1.engine_.loadConfig(engine1_config);
    player_2.engine_.loadConfig(engine2_config);

    player_1.engine_.startEngine(engine1_config.cmd);
    player_2.engine_.startEngine(engine2_config.cmd);

    data_.players.first.config = engine1_config;
    data_.players.second.config = engine2_config;

    data_.round = round;

    start(player_1, player_2, fen);
}

MatchData Match::get() const { return data_; }

void Match::setDraw(Participant& us, Participant& them, const std::string& msg) {
    data_.termination = msg;
    us.info_.result = GameResult::DRAW;
    them.info_.result = GameResult::DRAW;
}

void Match::setWin(Participant& us, Participant& them, const std::string& msg) {
    data_.termination = msg;
    us.info_.result = GameResult::WIN;
    them.info_.result = GameResult::LOSE;
}

void Match::setLose(Participant& us, Participant& them, const std::string& msg) {
    data_.termination = msg;
    us.info_.result = GameResult::LOSE;
    them.info_.result = GameResult::WIN;
}

void Match::addMoveData(Participant& player, int64_t measured_time) {
    const auto best_move = player.engine_.bestmove();

    MoveData move_data = MoveData(best_move, "0.00", measured_time, 0, 0, 0, 0);

    if (player.engine_.output().size() <= 1) {
        data_.moves.push_back(move_data);
        return;
    }

    // extract last info line
    const auto score_type = player.engine_.lastScoreType();
    const auto score = player.engine_.lastScore();
    const auto info = player.engine_.lastInfo();

    move_data.depth = findElement<int>(info, "depth").value_or(0);
    move_data.seldepth = findElement<int>(info, "seldepth").value_or(0);
    move_data.nodes = findElement<uint64_t>(info, "nodes").value_or(0);
    move_data.score = findElement<int>(info, score_type).value_or(0);

    // Missing elements default to 0
    std::stringstream ss;
    if (score_type == "cp") {
        ss << (move_data.score >= 0 ? '+' : '-');
        ss << std::fixed << std::setprecision(2) << (float(std::abs(move_data.score)) / 100);
    } else if (score_type == "mate") {
        ss << (move_data.score > 0 ? "+M" : "-M") << std::to_string(std::abs(move_data.score));
    } else {
        ss << "ERR";
    }

    move_data.score_string = ss.str();

    // verify pv
    for (const auto& info : player.engine_.output()) {
        const auto tokens = splitString(info, ' ');

        if (!contains(tokens, "moves")) continue;

        auto tmp = board_;
        auto it = std::find(tokens.begin(), tokens.end(), "pv");
        while (++it != tokens.end()) {
            if (Movegen::isLegal(tmp, board_.uciToMove(*it))) {
                std::stringstream ss;
                ss << "Warning: Illegal pv move " << *it << ".\n";
                std::cout << ss.str();
                break;
            }
        }
    }

    data_.moves.push_back(move_data);
}

void Match::start(Participant& engine1, Participant& engine2, const std::string& fen) {
    Board board(fen);

    start_fen_ = board.getFen() == STARTPOS ? "startpos" : board.getFen();

    engine1.engine_.sendUciNewGame();
    engine2.engine_.sendUciNewGame();

    // copy time control which will be updated later
    engine1.time_control_ = engine1.info_.config.limit.tc;
    engine2.time_control_ = engine2.info_.config.limit.tc;

    engine1.info_.color = board.sideToMove();
    engine2.info_.color = ~board.sideToMove();

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

    data_.players = std::make_pair(engine1.info_, engine2.info_);
}

bool Match::playMove(Participant& us, Participant& opponent) {
    const auto gameover = board_.isGameOver();

    if (gameover == GameResult::DRAW) {
        setDraw(us, opponent, "");
        return false;
    } else if (gameover == GameResult::LOSE) {
        setLose(us, opponent, "");
        return false;
    }

    if (!us.engine_.isResponsive()) {
        setLose(us, opponent, "timeout");
        return false;
    }

    us.engine_.writeEngine(us.engine_.buildPositionInput(played_moves_, start_fen_));
    us.engine_.writeEngine(
        us.engine_.buildGoInput(board_.sideToMove(), us.time_control_, opponent.time_control_));

    const auto t0 = clock::now();
    auto output = us.engine_.readEngine("bestmove", us.getTimeoutThreshold());
    const auto t1 = clock::now();

    const auto elapsed_millis = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

    if (!us.updateTc(elapsed_millis)) {
        setLose(us, opponent, "timeout");

        Logger::coutInfo("Warning: Engine", us.info_.config.name, "loses on time");

        return false;
    }

    const auto best_move = us.engine_.bestmove();
    const auto move = board_.uciToMove(best_move);

    if (!Movegen::isLegal(board_, move)) {
        setLose(us, opponent, "illegal move");

        Logger::coutInfo("Warning: Illegal move", best_move, "played by", us.info_.config.name);

        return false;
    }

    addMoveData(us, elapsed_millis);
    played_moves_.push_back(best_move);

    updateDrawTracker(us);
    updateResignTracker(us);

    return !adjudicate(us, opponent);
}

void Match::updateDrawTracker(const Participant& player) {
    const auto score = player.engine_.lastScore();

    if (played_moves_.size() >= game_config_.draw.move_number &&
        std::abs(score) <= game_config_.draw.score && player.engine_.lastScoreType() == "cp") {
        draw_tracker_.draw_moves++;
    } else {
        draw_tracker_.draw_moves = 0;
    }
}

void Match::updateResignTracker(const Participant& player) {
    const auto score = player.engine_.lastScore();

    if (std::abs(score) >= game_config_.resign.score && player.engine_.lastScoreType() == "cp") {
        resign_tracker_.resign_moves++;
    } else {
        resign_tracker_.resign_moves = 0;
    }
}

bool Match::adjudicate(Participant& us, Participant& them) {
    if (game_config_.draw.enabled && draw_tracker_.draw_moves >= game_config_.draw.move_count) {
        setDraw(us, them, "adjudication");
        return true;
    } else if (game_config_.resign.enabled &&
               resign_tracker_.resign_moves >= game_config_.resign.move_count) {
        if (us.engine_.lastScore() < game_config_.resign.score) {
            setLose(us, them, "resign adjudication");
        } else {
            setWin(us, them, "resign adjudication");
        }
        return true;
    }
    return false;
}

}  // namespace fast_chess