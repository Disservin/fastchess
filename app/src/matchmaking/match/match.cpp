#include <matchmaking/match/match.hpp>

#include <algorithm>

#include <types/tournament.hpp>
#include <util/date.hpp>
#include <util/helper.hpp>
#include <util/logger/logger.hpp>

namespace fastchess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

namespace {
bool isFen(const std::string& line) { return line.find(';') == std::string::npos; }
}  // namespace

namespace chrono = std::chrono;

using clock = chrono::high_resolution_clock;
using namespace std::literals;
using namespace chess;

std::string Match::convertScoreToString(int score, engine::ScoreType score_type) {
    std::stringstream ss;

    if (score_type == engine::ScoreType::CP) {
        ss << (score > 0 ? "+" : score < 0 ? "-" : "");
        ss << std::fixed << std::setprecision(2) << (float(std::abs(score)) / 100);
    } else if (score_type == engine::ScoreType::MATE) {
        uint64_t plies = score > 0 ? score * 2 - 1 : score * -2;
        ss << (score > 0 ? "+M" : "-M") << std::to_string(plies);
    } else {
        ss << "ERR";
    }

    return ss.str();
}

void Match::addMoveData(const Player& player, int64_t measured_time_ms, int64_t timeleft, bool legal) {
    const auto move = player.engine.bestmove().value_or("<none>");

    MoveData move_data = MoveData(move, "0.00", measured_time_ms, 0, 0, 0, 0, legal);

    if (player.engine.output().size() <= 1) {
        data_.moves.push_back(move_data);
        uci_moves_.push_back(move);
        return;
    }

    // extract last info line
    const auto score_type = player.engine.lastScoreType();
    const auto info       = player.engine.lastInfo();

    move_data.nps      = str_utils::findElement<int>(info, "nps").value_or(0);
    move_data.hashfull = str_utils::findElement<int>(info, "hashfull").value_or(0);
    move_data.tbhits   = str_utils::findElement<uint64_t>(info, "tbhits").value_or(0);
    move_data.depth    = str_utils::findElement<int>(info, "depth").value_or(0);
    move_data.seldepth = str_utils::findElement<int>(info, "seldepth").value_or(0);
    move_data.nodes    = str_utils::findElement<uint64_t>(info, "nodes").value_or(0);
    move_data.score    = player.engine.lastScore();
    move_data.timeleft = timeleft;

    move_data.score_string = Match::convertScoreToString(move_data.score, score_type);

    verifyPvLines(player);

    data_.moves.push_back(move_data);
    uci_moves_.push_back(move);
}

void Match::prepare() {
    board_.set960(config::TournamentConfig.get().variant == VariantType::FRC);

    if (isFen(opening_.fen_epd))
        board_.setFen(opening_.fen_epd);
    else
        board_.setEpd(opening_.fen_epd);

    start_position_ = board_.getFen() == chess::constants::STARTPOS ? "startpos" : board_.getFen();

    const auto insert_move = [&](const auto& opening_move) {
        const auto move = uci::moveToUci(opening_move, board_.chess960());
        board_.makeMove(opening_move);

        return MoveData(move, "0.00", 0, 0, 0, 0, 0, true, true);
    };

    data_ = MatchData(board_.getFen());

    std::transform(opening_.moves.begin(), opening_.moves.end(), std::back_inserter(data_.moves), insert_move);

    draw_tracker_     = DrawTracker();
    resign_tracker_   = ResignTracker();
    maxmoves_tracker_ = MaxMovesTracker();
}

void Match::start(engine::UciEngine& white, engine::UciEngine& black, const std::vector<int>& cpus) {
    prepare();

    std::transform(data_.moves.begin(), data_.moves.end(), std::back_inserter(uci_moves_),
                   [](const MoveData& data) { return data.move; });

    Player white_player = Player(white);
    Player black_player = Player(black);

    white_player.color = Color::WHITE;
    black_player.color = Color::BLACK;

    if (!white_player.engine.start()) {
        Logger::trace<true>("Failed to start engines, stopping tournament.");
        atomic::stop = true;
        return;
    }

    if (atomic::stop.load()) {
        return;
    }

    if (!black_player.engine.start()) {
        Logger::trace<true>("Failed to start engines, stopping tournament.");
        atomic::stop = true;
        return;
    }

    white_player.engine.refreshUci();
    black_player.engine.refreshUci();

    white_player.engine.setCpus(cpus);
    black_player.engine.setCpus(cpus);

    auto& first  = board_.sideToMove() == Color::WHITE ? white_player : black_player;
    auto& second = board_.sideToMove() == Color::WHITE ? black_player : white_player;

    const auto start = clock::now();

    try {
        while (true) {
            if (atomic::stop.load()) {
                data_.termination = MatchTermination::INTERRUPT;
                break;
            }

            if (!playMove(first, second)) break;

            if (atomic::stop.load()) {
                data_.termination = MatchTermination::INTERRUPT;
                break;
            }

            if (!playMove(second, first)) break;
        }
    } catch (const std::exception& e) {
    }

    const auto end = clock::now();

    data_.variant = config::TournamentConfig.get().variant;

    data_.end_time = util::time::datetime("%Y-%m-%dT%H:%M:%S %z");
    data_.duration = util::time::duration(chrono::duration_cast<chrono::seconds>(end - start));

    data_.players =
        GamePair(MatchData::PlayerInfo{white_player.engine.getConfig(), white_player.getResult(), white_player.color},
                 MatchData::PlayerInfo{black_player.engine.getConfig(), black_player.getResult(), black_player.color});
}

bool Match::playMove(Player& us, Player& them) {
    const auto gameover = isGameOver();
    const auto name     = us.engine.getConfig().name;

    if (gameover.second == GameResult::DRAW) {
        us.setDraw();
        them.setDraw();
    }

    if (gameover.second == GameResult::LOSE) {
        us.setLost();
        them.setWon();
    }

    if (gameover.first != GameResultReason::NONE) {
        data_.termination = MatchTermination::NORMAL;
        data_.reason      = convertChessReason(getColorString(), gameover.first);
        return false;
    }

    // make sure adjudicate is placed after normal termination as it has lower priority
    if (adjudicate(them, us)) {
        return false;
    }

    // make sure the engine is not in an invalid state
    if (!validConnection(us, them)) return false;

    // write new uci position
    auto success = us.engine.position(uci_moves_, start_position_);
    if (!success) {
        setEngineCrashStatus(us, them);
        return false;
    }

    // make sure the engine is not in an invalid state
    // after writing the position
    if (!validConnection(us, them)) return false;

    // write go command
    Logger::trace<true>("Engine {} is thinking", name);
    success = us.engine.go(us.getTimeControl(), them.getTimeControl(), board_.sideToMove());
    if (!success) {
        setEngineCrashStatus(us, them);
        return false;
    }

    // wait for bestmove
    auto t0     = clock::now();
    auto status = us.engine.readEngine("bestmove", us.getTimeoutThreshold());
    auto t1     = clock::now();

    Logger::trace<true>("Engine {} is done thinking", name);

    if (!config::TournamentConfig.get().log.realtime) {
        us.engine.writeLog();
    }

    if (atomic::stop) {
        data_.termination = MatchTermination::INTERRUPT;

        return false;
    }

    Logger::trace<true>("Check if engine {} is in a ready state", name);

    if (status == engine::process::Status::ERR) {
        setEngineCrashStatus(us, them);
        return false;
    }

    // make sure the engine is not in an invalid state after
    // the search completed
    if (!validConnection(us, them)) return false;

    Logger::trace<true>("Engine {} is in a ready state", name);

    const auto elapsed_millis = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

    const auto best_move = us.engine.bestmove();
    const auto move      = best_move ? uci::uciToMove(board_, *best_move) : Move::NO_MOVE;
    const auto legal     = isLegal(move);

    const auto timeout  = !us.updateTime(elapsed_millis);
    const auto timeleft = us.getTimeControl().getTimeLeft();

    if (best_move) {
        addMoveData(us, elapsed_millis, timeleft, legal && isUciMove(best_move.value()));
    }

    // there are two reasons why best_move could be empty
    // 1. the engine crashed
    // 2. the engine did not respond in time
    // we report a loss on time when the engine didnt respond in time
    // and otherwise an illegal move
    if (best_move == std::nullopt) {
        // Time forfeit
        if (timeout) {
            setEngineTimeoutStatus(us, them);
        } else {
            setEngineIllegalMoveStatus(us, them, best_move);
        }

        return false;
    }

    if (best_move && !isUciMove(best_move.value())) {
        setEngineIllegalMoveStatus(us, them, best_move, true);
        return false;
    }

    // illegal move
    if (!legal) {
        setEngineIllegalMoveStatus(us, them, best_move);
        return false;
    }

    if (timeout) {
        setEngineTimeoutStatus(us, them);
        return false;
    }

    board_.makeMove(move);

    // CuteChess uses plycount/2 for its movenumber, which is wrong for epd books as it doesnt take
    // into account the fullmove counter of the starting FEN, leading to different behavior between
    // pgn and epd adjudication. fastchess fixes this by using the fullmove counter from the board
    // object directly
    auto score = us.engine.lastScore();
    auto type  = us.engine.lastScoreType();

    draw_tracker_.update(score, type, board_.halfMoveClock());
    resign_tracker_.update(score, type, ~board_.sideToMove());
    maxmoves_tracker_.update();

    return true;
}

bool Match::validConnection(Player& us, Player& them) {
    const auto is_ready = us.engine.isready();

    if (is_ready == engine::process::Status::TIMEOUT) {
        setEngineStallStatus(us, them);
        return false;
    }

    if (is_ready != engine::process::Status::OK) {
        setEngineCrashStatus(us, them);
        return false;
    }

    return true;
}

bool Match::isLegal(Move move) const noexcept {
    Movelist moves;
    movegen::legalmoves(moves, board_);

    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

std::pair<chess::GameResultReason, chess::GameResult> Match::isGameOver() const {
    Movelist movelist;
    movegen::legalmoves(movelist, board_);

    if (movelist.empty()) {
        if (board_.inCheck()) return {GameResultReason::CHECKMATE, GameResult::LOSE};
        return {GameResultReason::STALEMATE, GameResult::DRAW};
    }

    if (board_.isInsufficientMaterial()) return {GameResultReason::INSUFFICIENT_MATERIAL, GameResult::DRAW};
    if (board_.isHalfMoveDraw()) return board_.getHalfMoveDrawType();
    if (board_.isRepetition()) return {GameResultReason::THREEFOLD_REPETITION, GameResult::DRAW};

    return {GameResultReason::NONE, GameResult::NONE};
}

void Match::setEngineCrashStatus(Player& loser, Player& winner) {
    loser.setLost();
    winner.setWon();

    stall_or_disconnect_ = true;

    const auto name  = loser.engine.getConfig().name;
    const auto color = getColorString();

    data_.termination = MatchTermination::DISCONNECT;
    data_.reason      = color + Match::DISCONNECT_MSG;

    Logger::trace<true>("Engine {} disconnects", name);
}

void Match::setEngineStallStatus(Player& loser, Player& winner) {
    loser.setLost();
    winner.setWon();

    stall_or_disconnect_ = true;

    const auto name  = loser.engine.getConfig().name;
    const auto color = getColorString();

    data_.termination = MatchTermination::STALL;
    data_.reason      = color + Match::STALL_MSG;

    Logger::trace<true>("Engine {}'s connection stalls", name);
}

void Match::setEngineTimeoutStatus(Player& loser, Player& winner) {
    loser.setLost();
    winner.setWon();

    const auto name  = loser.engine.getConfig().name;
    const auto color = getColorString();

    data_.termination = MatchTermination::TIMEOUT;
    data_.reason      = color + Match::TIMEOUT_MSG;

    Logger::trace<true>("Engine {} loses on time", name);

    // we send a stop command to the engine to prevent it from thinking
    // and wait for a bestmove to appear

    loser.engine.writeEngine("stop");

    if (!loser.engine.outputIncludesBestmove()) {
        // wait 10 seconds for the bestmove to appear
        loser.engine.readEngine("bestmove", 1000ms * 10);
    }
}

void Match::setEngineIllegalMoveStatus(Player& loser, Player& winner, const std::optional<std::string>& best_move,
                                       bool invalid_format) {
    loser.setLost();
    winner.setWon();

    const auto name  = loser.engine.getConfig().name;
    const auto color = getColorString();

    data_.termination = MatchTermination::ILLEGAL_MOVE;
    data_.reason      = color + Match::ILLEGAL_MSG;

    auto mv = best_move.value_or("<none>");

    if (invalid_format) {
        Logger::warn<true>(
            "Warning; Move does not match uci move format, lowercase and 4/5 chars. Move {} played by {}", mv, name);
    }

    Logger::warn<true>("Warning; Illegal move {} played by {}", mv, name);
}

bool Match::isUciMove(const std::string& move) noexcept {
    bool is_uci = false;

    constexpr auto is_digit     = [](char c) { return c >= '0' && c <= '9'; };
    constexpr auto is_file      = [](char c) { return c >= 'a' && c <= 'h'; };
    constexpr auto is_promotion = [](char c) { return c == 'n' || c == 'b' || c == 'r' || c == 'q'; };

    // assert that the move is in uci format, [abcdefgh][0-9][abcdefgh][0-9][nbrq]
    if (move.size() >= 4) {
        is_uci = is_file(move[0]) && is_digit(move[1]) && is_file(move[2]) && is_digit(move[3]);
    }

    if (move.size() == 5) {
        is_uci = is_uci && is_promotion(move[4]);
    }

    if (move.size() > 5) {
        return false;
    }

    return is_uci;
}

void Match::verifyPvLines(const Player& us) {
    const static auto verifyPv = [](Board board, const std::string& startpos, const std::vector<std::string>& uci_moves,
                                    const std::string& info) {
        // skip lines without pv
        const auto tokens = str_utils::splitString(info, ' ');
        if (!str_utils::contains(tokens, "pv")) return;

        const auto fen = board.getFen();
        auto it_start  = std::find(tokens.begin(), tokens.end(), "pv") + 1;
        auto it_end    = std::find_if(it_start, tokens.end(), [](const auto& token) { return !isUciMove(token); });

        Movelist moves;

        while (it_start != it_end) {
            movegen::legalmoves(moves, board);

            if (std::find(moves.begin(), moves.end(), uci::uciToMove(board, *it_start)) == moves.end()) {
                auto fmt      = fmt::format("Warning; Illegal pv move {} pv: {}", *it_start, info);
                auto position = fmt::format("position {}", startpos == "startpos" ? "startpos" : ("fen " + startpos));
                auto fmt2     = fmt::format("From; {} moves {}", position, str_utils::join(uci_moves, " "));

                Logger::warn<true>("{} \n {}", fmt, "\n", fmt2);

                break;
            }

            board.makeMove(uci::uciToMove(board, *it_start));

            it_start++;
        }
    };

    for (const auto& info : us.engine.output()) {
        verifyPv(board_, start_position_, uci_moves_, info.line);
    }
}

bool Match::adjudicate(Player& us, Player& them) noexcept {
    if (config::TournamentConfig.get().resign.enabled && resign_tracker_.resignable() && us.engine.lastScore() < 0) {
        us.setLost();
        them.setWon();

        const auto color = getColorString(board_.sideToMove());

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = color + Match::ADJUDICATION_WIN_MSG;

        return true;
    }

    if (config::TournamentConfig.get().draw.enabled && draw_tracker_.adjudicatable(board_.fullMoveNumber() - 1)) {
        us.setDraw();
        them.setDraw();

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = Match::ADJUDICATION_MSG;

        return true;
    }

    if (config::TournamentConfig.get().maxmoves.enabled && maxmoves_tracker_.maxmovesreached()) {
        us.setDraw();
        them.setDraw();

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = Match::ADJUDICATION_MSG;

        return true;
    }

    return false;
}

std::string Match::convertChessReason(const std::string& engine_color, GameResultReason reason) noexcept {
    if (reason == GameResultReason::CHECKMATE) {
        std::string color = engine_color == "White" ? "Black" : "White";
        return color + Match::CHECKMATE_MSG;
    }

    if (reason == GameResultReason::STALEMATE) {
        return Match::STALEMATE_MSG;
    }

    if (reason == GameResultReason::INSUFFICIENT_MATERIAL) {
        return Match::INSUFFICIENT_MSG;
    }

    if (reason == GameResultReason::THREEFOLD_REPETITION) {
        return Match::REPETITION_MSG;
    }

    if (reason == GameResultReason::FIFTY_MOVE_RULE) {
        return Match::FIFTY_MSG;
    }

    return "";
}

}  // namespace fastchess
