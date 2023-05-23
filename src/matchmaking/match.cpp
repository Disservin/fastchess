#include <matchmaking/match.hpp>

#include <helper.hpp>
#include <logger.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

using namespace std::literals;
namespace chrono = std::chrono;
using clock = chrono::high_resolution_clock;

using namespace chess;

Match::Match(const cmd::GameManagerOptions& game_config, const EngineConfiguration& engine1_config,
             const EngineConfiguration& engine2_config, const Opening& opening, int round) {
    game_config_ = game_config;
    data_.round = round;

    engine1_config_ = engine1_config;
    engine2_config_ = engine2_config;

    opening_ = opening;
}

MatchData Match::get() const { return data_; }

void Match::verifyPv(const Participant& us) {
    for (const auto& info : us.engine.output()) {
        const auto tokens = StrUtil::splitString(info, ' ');

        if (!StrUtil::contains(tokens, "moves")) continue;

        auto tmp = board_;
        auto it = std::find(tokens.begin(), tokens.end(), "pv");
        while (++it != tokens.end()) {
            if (movegen::isLegal<chess::Move>(tmp, board_.uciToMove(*it))) {
                Logger::cout("Warning: Illegal pv move ", *it);
                break;
            }
        }
    }
}

void Match::setDraw(Participant& us, Participant& them, const std::string& msg,
                    const std::string& reason) {
    data_.internal_reason = reason;
    data_.termination = msg;
    us.info.result = GameResult::DRAW;
    them.info.result = GameResult::DRAW;
}

void Match::setWin(Participant& us, Participant& them, const std::string& msg,
                   const std::string& reason) {
    data_.internal_reason = reason;
    data_.termination = msg;
    us.info.result = GameResult::WIN;
    them.info.result = GameResult::LOSE;
}

void Match::setLose(Participant& us, Participant& them, const std::string& msg,
                    const std::string& reason) {
    data_.internal_reason = reason;
    data_.termination = msg;
    us.info.result = GameResult::LOSE;
    them.info.result = GameResult::WIN;
}

void Match::addMoveData(Participant& player, int64_t measured_time) {
    const auto best_move = player.engine.bestmove();

    MoveData move_data = MoveData(best_move, "0.00", measured_time, 0, 0, 0, 0);

    if (player.engine.output().size() <= 1) {
        data_.moves.push_back(move_data);
        return;
    }

    // extract last info line
    const auto score_type = player.engine.lastScoreType();
    const auto info = player.engine.lastInfo();

    move_data.depth = StrUtil::findElement<int>(info, "depth").value_or(0);
    move_data.seldepth = StrUtil::findElement<int>(info, "seldepth").value_or(0);
    move_data.nodes = StrUtil::findElement<uint64_t>(info, "nodes").value_or(0);
    move_data.score = player.engine.lastScore();

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
    verifyPv(player);

    data_.moves.push_back(move_data);
    played_moves_.push_back(best_move);
}

void Match::start() {
    Participant player_1 = Participant(engine1_config_);
    Participant player_2 = Participant(engine2_config_);

    player_1.engine.startEngine();
    player_2.engine.startEngine();

    if (!player_1.engine.sendUciNewGame()) {
        throw std::runtime_error(player_1.engine.getConfig().name + " failed to start.");
    }

    if (!player_2.engine.sendUciNewGame()) {
        throw std::runtime_error(player_2.engine.getConfig().name + " failed to start.");
    }

    board_.loadFen(opening_.fen);

    std::vector<std::string> uci_moves = [&]() {
        Board board = board_;
        std::vector<std::string> moves;
        for (const auto& move : opening_.moves) {
            Move i_move = board_.parseSan(move);
            moves.push_back(board.moveToUci(i_move));
            board_.makeMove(i_move);
        }
        return moves;
    }();

    played_moves_.insert(played_moves_.end(), uci_moves.begin(), uci_moves.end());

    start_fen_ = board_.getFen() == STARTPOS ? "startpos" : board_.getFen();

    // copy time control which will be updated later
    player_1.time_control = player_1.engine.getConfig().limit.tc;
    player_2.time_control = player_2.engine.getConfig().limit.tc;

    player_1.info.color = board_.sideToMove();
    player_2.info.color = ~board_.sideToMove();

    data_.fen = opening_.fen;

    data_.start_time = Logger::getDateTime();
    data_.date = Logger::getDateTime("%Y-%m-%d");

    auto start = clock::now();
    try {
        while (true) {
            if (atomic::stop.load()) {
                data_.termination = "stop";
                break;
            };
            if (!playMove(player_1, player_2)) break;

            if (atomic::stop.load()) {
                data_.termination = "stop";
                break;
            };
            if (!playMove(player_2, player_1)) break;
        }
    } catch (const std::exception& e) {
        if (game_config_.recover)
            data_.needs_restart = true;
        else {
            throw e;
        }
    }
    auto end = clock::now();

    data_.end_time = Logger::getDateTime();
    data_.duration = Logger::formatDuration(chrono::duration_cast<chrono::seconds>(end - start));

    data_.players = std::make_pair(player_1.info, player_2.info);
}

bool Match::playMove(Participant& us, Participant& opponent) {
    const auto gameover = board_.isGameOver();
    const auto name = us.engine.getConfig().name;

    if (gameover.second == GameResult::DRAW) {
        setDraw(us, opponent, "", convertChessReason(name, gameover.first));
        return false;
    } else if (gameover.second == GameResult::LOSE) {
        setLose(us, opponent, "", convertChessReason(name, gameover.first));
        return false;
    }

    if (!us.engine.isResponsive()) {
        setLose(us, opponent, "timeout", name + Match::TIMEOUT_MSG);
        return false;
    }

    auto position = us.engine.buildPositionInput(played_moves_, start_fen_);
    auto go = us.engine.buildGoInput(board_.sideToMove(), us.time_control, opponent.time_control);

    us.engine.writeEngine(position);
    us.engine.writeEngine(go);

    const auto t0 = clock::now();
    auto output = us.engine.readEngine("bestmove", us.getTimeoutThreshold());
    const auto t1 = clock::now();

    const auto elapsed_millis = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

    if (!us.updateTime(elapsed_millis)) {
        setLose(us, opponent, "timeout", name + Match::TIMEOUT_MSG);

        Logger::cout("Warning: Engine", name, "loses on time");

        return false;
    }

    const auto best_move = us.engine.bestmove();
    const auto move = board_.uciToMove(best_move);

    if (!movegen::isLegal<chess::Move>(board_, move)) {
        setLose(us, opponent, "illegal move", name + Match::ILLEGAL_MSG);

        Logger::cout("Warning: Illegal move", best_move, "played by", name);

        return false;
    }

    updateDrawTracker(us);
    updateResignTracker(us);

    addMoveData(us, elapsed_millis);

    board_.makeMove(move);

    return !adjudicate(us, opponent);
}

void Match::updateDrawTracker(const Participant& player) {
    const auto score = player.engine.lastScore();

    if (played_moves_.size() >= game_config_.draw.move_number &&
        std::abs(score) <= game_config_.draw.score && player.engine.lastScoreType() == "cp") {
        draw_tracker_.draw_moves++;
    } else {
        draw_tracker_.draw_moves = 0;
    }
}

void Match::updateResignTracker(const Participant& player) {
    const auto score = player.engine.lastScore();

    if (std::abs(score) >= game_config_.resign.score && player.engine.lastScoreType() == "cp") {
        resign_tracker_.resign_moves++;
    } else {
        resign_tracker_.resign_moves = 0;
    }
}

bool Match::adjudicate(Participant& us, Participant& them) {
    if (game_config_.draw.enabled && draw_tracker_.draw_moves >= game_config_.draw.move_count) {
        setDraw(us, them, "adjudication", Match::ADJUDICATION_MSG);
        return true;
    } else if (game_config_.resign.enabled &&
               resign_tracker_.resign_moves >= game_config_.resign.move_count) {
        if (us.engine.lastScore() < game_config_.resign.score) {
            setLose(us, them, "resign adjudication",
                    us.engine.getConfig().name + Match::ADJUDICATION_LOSE_MSG);
        } else {
            setWin(us, them, "resign adjudication",
                   us.engine.getConfig().name + Match::ADJUDICATION_WIN_MSG);
        }
        return true;
    }
    return false;
}

std::string Match::convertChessReason(const std::string& engine_name, std::string_view reason) {
    if (reason == "checkmate") {
        return engine_name + Match::CHECKMATE_MSG;
    }
    if (reason == "stalemate") {
        return Match::STALEMATE_MSG;
    }
    if (reason == "insufficient material") {
        return Match::INSUFFICIENT_MSG;
    }
    if (reason == "threefold repetition") {
        return Match::REPETITION_MSG;
    }
    if (reason == "50 move rule") {
        return Match::FIFTY_MSG;
    }
    return "";
}

}  // namespace fast_chess