#include <matchmaking/match/match.hpp>

#include <algorithm>
#include <regex>

#include <core/globals/globals.hpp>
#include <core/helper.hpp>
#include <core/logger/logger.hpp>
#include <core/time/time.hpp>
#include <types/tournament.hpp>

namespace fastchess {

namespace chrono = std::chrono;

using namespace std::literals;
using namespace chess;
using clock = chrono::steady_clock;

namespace {

std::string to_escaped_string(const std::string& binary_str) {
    std::stringstream ss;

    for (unsigned char c : binary_str) {
        if (c >= 32 && c <= 126) {
            ss << c;
        } else {
            ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }

    return ss.str();
}

bool isFen(const std::string& line) { return line.find(';') == std::string::npos; }

[[nodiscard]] std::pair<GameResultReason, GameResult> isGameOverSimple(const Board& board) {
    if (board.isHalfMoveDraw()) return board.getHalfMoveDrawType();
    if (board.isRepetition()) return {GameResultReason::THREEFOLD_REPETITION, GameResult::DRAW};

    Movelist movelist;
    movegen::legalmoves(movelist, board);

    if (movelist.empty()) {
        if (board.inCheck()) return {GameResultReason::CHECKMATE, GameResult::LOSE};
        return {GameResultReason::STALEMATE, GameResult::DRAW};
    }

    return {GameResultReason::NONE, GameResult::NONE};
}

}  // namespace

Match::Match(const book::Opening& opening)
    : opening_(opening),
      draw_tracker_(config::TournamentConfig->draw.move_number, config::TournamentConfig->draw.move_count,
                    config::TournamentConfig->draw.score),
      resign_tracker_(config::TournamentConfig->resign.score, config::TournamentConfig->resign.move_count,
                      config::TournamentConfig->resign.twosided),
      maxmoves_tracker_(config::TournamentConfig->maxmoves.move_count),
      tb_adjudication_tracker_(config::TournamentConfig->tb_adjudication.max_pieces,
                               config::TournamentConfig->tb_adjudication.ignore_50_move_rule) {
    board_.set960(config::TournamentConfig->variant == VariantType::FRC);

    const auto success = isFen(opening_.fen_epd) ? board_.setFen(opening_.fen_epd) : board_.setEpd(opening_.fen_epd);

    if (!success) {
        LOG_FATAL_THREAD("Failed to set board position from opening book");
        atomic::stop                 = true;
        atomic::abnormal_termination = true;
        auto fen                     = to_escaped_string(opening_.fen_epd);

        Logger::print<Logger::Level::FATAL>("Failed to set position from opening book, invalid FEN or EPD: {}", fen);
        throw std::runtime_error("Failed to set position from opening book, invalid FEN or EPD: " + fen);
    }

    const auto fen = board_.getFen();

    data_           = MatchData(fen);
    start_position_ = fen == chess::constants::STARTPOS ? "startpos" : fen;

    const auto insert_move = [&](const auto& opening_move) {
        const auto move = uci::moveToUci(opening_move, board_.chess960());
        board_.makeMove(opening_move);

        return MoveData(move, "0.00", 0, 0, 0, 0, 0, true, true);
    };

    std::transform(opening_.moves.begin(), opening_.moves.end(), std::back_inserter(data_.moves), insert_move);
}

std::string Match::convertScoreToString(int score, engine::ScoreType score_type) {
    if (score_type == engine::ScoreType::CP) {
        float normalized_score = static_cast<float>(std::abs(score)) / 100;
        return fmt::format("{}{:.2f}", score > 0 ? "+" : score < 0 ? "-" : "", normalized_score);
    } else if (score_type == engine::ScoreType::MATE) {
        uint64_t plies = score > 0 ? score * 2 - 1 : score * -2;
        return fmt::format("{}M{}", score > 0 ? "+" : "-", plies);
    }

    return "ERR";
}

void Match::addMoveData(const Player& player, int64_t measured_time_ms, int64_t latency, int64_t timeleft, bool legal) {
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

    move_data.nps      = str_utils::findElement<uint64_t>(info, "nps").value_or(0);
    move_data.hashfull = str_utils::findElement<int>(info, "hashfull").value_or(0);
    move_data.tbhits   = str_utils::findElement<uint64_t>(info, "tbhits").value_or(0);
    move_data.depth    = str_utils::findElement<int>(info, "depth").value_or(0);
    move_data.seldepth = str_utils::findElement<int>(info, "seldepth").value_or(0);
    move_data.nodes    = str_utils::findElement<uint64_t>(info, "nodes").value_or(0);
    move_data.score    = player.engine.lastScore();
    move_data.timeleft = timeleft;
    move_data.latency  = latency;

    move_data.score_string = Match::convertScoreToString(move_data.score, score_type);

    if (!config::TournamentConfig->pgn.additional_lines_rgx.empty()) {
        for (const auto& rgx : config::TournamentConfig->pgn.additional_lines_rgx) {
            const auto lines = player.engine.output();
            const auto regex = std::regex(rgx);
            // find the last line that matches the regex, iterate in reverse
            for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                if (std::regex_search(it->line, regex)) {
                    move_data.additional_lines.push_back(it->line);
                    break;
                }
            }
        }
    }

    verifyPvLines(player);

    data_.moves.push_back(move_data);
    uci_moves_.push_back(move);
}

void Match::start(engine::UciEngine& white, engine::UciEngine& black, const std::vector<int>& cpus) {
    std::transform(data_.moves.begin(), data_.moves.end(), std::back_inserter(uci_moves_),
                   [](const MoveData& data) { return data.move; });

    Player white_player = Player(white);
    Player black_player = Player(black);

    if (!white_player.engine.start()) {
        LOG_FATAL_THREAD("Failed to start engines, stopping tournament.");
        atomic::stop                 = true;
        atomic::abnormal_termination = true;
        return;
    }

    if (atomic::stop.load()) {
        return;
    }

    if (!black_player.engine.start()) {
        LOG_FATAL_THREAD("Failed to start engines, stopping tournament.");
        atomic::stop                 = true;
        atomic::abnormal_termination = true;
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
        LOG_FATAL_THREAD("Match failed with exception; {}", e.what());
    }

    const auto end = clock::now();

    data_.variant = config::TournamentConfig->variant;

    data_.end_time = time::datetime("%Y-%m-%dT%H:%M:%S %z").value_or("-");
    data_.duration = time::duration(chrono::duration_cast<chrono::seconds>(end - start));

    data_.players = GamePair(MatchData::PlayerInfo{white_player.engine.getConfig(), white_player.getResult()},
                             MatchData::PlayerInfo{black_player.engine.getConfig(), black_player.getResult()});
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
        data_.reason      = convertChessReason((~board_.sideToMove()).longStr(), gameover.first);
        return false;
    }

    // make sure adjudicate is placed after normal termination as it has lower priority
    if (adjudicate(them, us)) {
        return false;
    }

    // make sure the engine is not in an invalid state
    if (!validConnection(us, them)) return false;

    // write new uci position
    if (!us.engine.position(uci_moves_, start_position_)) {
        setEngineCrashStatus(us, them);
        return false;
    }

    // make sure the engine is not in an invalid state
    // after writing the position
    if (!validConnection(us, them)) return false;

    // write go command
    LOG_TRACE_THREAD("Engine {} is thinking", name);
    if (!us.engine.go(us.getTimeControl(), them.getTimeControl(), board_.sideToMove())) {
        setEngineCrashStatus(us, them);
        return false;
    }

    // prepare the engine for reading
    us.engine.setupReadEngine();

    // wait for bestmove
    const auto t0     = clock::now();
    const auto status = us.engine.readEngineLowLat("bestmove", us.getTimeoutThreshold());
    const auto t1     = clock::now();

    LOG_TRACE_THREAD("Engine {} is done thinking", name);

    if (!config::TournamentConfig->log.realtime) {
        us.engine.writeLog();
    }

    if (atomic::stop) {
        data_.termination = MatchTermination::INTERRUPT;

        return false;
    }

    LOG_TRACE_THREAD("Check if engine {} is in a ready state", name);

    if (status == engine::process::Status::ERR) {
        setEngineCrashStatus(us, them);
        return false;
    }

    // make sure the engine is not in an invalid state after
    // the search completed
    if (!validConnection(us, them)) return false;

    LOG_TRACE_THREAD("Engine {} is in a ready state", name);

    const auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

    // calculate latency
    const auto last_time = us.engine.lastTime().count();
    const auto latency   = elapsed_ms - last_time;
    if (config::TournamentConfig->show_latency) {
        LOG_INFO_THREAD("Engine {} latency: {}ms (elapsed: {}, reported: {})", name, latency, elapsed_ms, last_time);
    }

    const auto best_move = us.engine.bestmove();
    const auto move      = best_move ? uci::uciToMove(board_, *best_move) : Move::NO_MOVE;
    const auto legal     = isLegal(move);

    const auto timeout  = !us.updateTime(elapsed_ms);
    const auto timeleft = us.getTimeControl().getTimeLeft();

    if (best_move) {
        addMoveData(us, elapsed_ms, latency, timeleft, legal && uci::isUciMove(best_move.value()));
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

    if (best_move && !uci::isUciMove(best_move.value())) {
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
    const auto color = board_.sideToMove().longStr();

    if (atomic::stop) {
        data_.termination = MatchTermination::INTERRUPT;
        data_.reason      = Match::INTERRUPTED_MSG;
    } else {
        data_.termination = MatchTermination::DISCONNECT;
        data_.reason      = color + Match::DISCONNECT_MSG;
    }

    LOG_WARN_THREAD("Engine {} disconnects", name);
}

void Match::setEngineStallStatus(Player& loser, Player& winner) {
    loser.setLost();
    winner.setWon();

    stall_or_disconnect_ = true;

    const auto name  = loser.engine.getConfig().name;
    const auto color = board_.sideToMove().longStr();

    data_.termination = MatchTermination::STALL;
    data_.reason      = color + Match::STALL_MSG;

    LOG_WARN_THREAD("Engine {} stalls", name);
}

void Match::setEngineTimeoutStatus(Player& loser, Player& winner) {
    loser.setLost();
    winner.setWon();

    const auto name  = loser.engine.getConfig().name;
    const auto color = board_.sideToMove().longStr();

    data_.termination = MatchTermination::TIMEOUT;
    data_.reason      = color + Match::TIMEOUT_MSG;

    LOG_WARN_THREAD("Engine {} loses on time", name);

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
    const auto color = board_.sideToMove().longStr();

    data_.termination = MatchTermination::ILLEGAL_MOVE;
    data_.reason      = color + Match::ILLEGAL_MSG;

    auto mv = best_move.value_or("<none>");

    if (invalid_format) {
        Logger::print<Logger::Level::WARN>(
            "Warning; Move does not match uci move format, lowercase and 4/5 chars. Move {} played by {}", mv, name);
    }

    Logger::print<Logger::Level::WARN>("Warning; Illegal move {} played by {}", mv, name);
}

void Match::verifyPvLines(const Player& us) {
    const static auto verifyPv = [](Board board, const std::string& startpos, const std::vector<std::string>& uci_moves,
                                    const std::string& info, std::string_view name) {
        // skip lines without pv
        const auto tokens = str_utils::splitString(info, ' ');
        if (!str_utils::contains(tokens, "pv")) return;

        auto it_start = std::find(tokens.begin(), tokens.end(), "pv") + 1;
        auto it_end   = std::find_if(it_start, tokens.end(), [](const auto& token) { return !uci::isUciMove(token); });

        Movelist moves;

        while (it_start != it_end) {
            moves.clear();

            const auto gameover = isGameOverSimple(board).second != GameResult::NONE;

            if (!gameover) {
                movegen::legalmoves(moves, board);
            }

            if (gameover || std::find(moves.begin(), moves.end(), uci::uciToMove(board, *it_start)) == moves.end()) {
                auto out      = fmt::format("Warning; Illegal pv move {} from {}", *it_start, name);
                auto uci_info = fmt::format("Info; {}", info);
                auto position = fmt::format("Position; {}", startpos == "startpos" ? "startpos" : ("fen " + startpos));
                auto moves    = fmt::format("Moves; {}", str_utils::join(uci_moves, " "));

                auto separator = config::TournamentConfig->test_env ? " :: " : "\n";

                Logger::print<Logger::Level::WARN>("{1}{0}{2}{0}{3}{0}{4}", separator, out, uci_info, position, moves);

                break;
            }

            board.makeMove(uci::uciToMove(board, *it_start));

            it_start++;
        }
    };

    for (const auto& info : us.engine.output()) {
        verifyPv(board_, start_position_, uci_moves_, info.line, us.engine.getConfig().name);
    }
}

bool Match::adjudicate(Player& us, Player& them) noexcept {
    // Start with TB adjudication, if applicable, since this provides a sort of 'exact' result, whereas the other
    // adjudication methods are more heuristic.
    if (config::TournamentConfig->tb_adjudication.enabled && tb_adjudication_tracker_.adjudicatable(board_)) {
        const GameResult result = tb_adjudication_tracker_.adjudicate(board_);
        if (result != GameResult::NONE) {
            // Note: 'them' is the player to move at this point, so the result is from their perspective.

            if (result == GameResult::DRAW) {
                us.setDraw();
                them.setDraw();

                data_.reason = Match::ADJUDICATION_TB_DRAW_MSG;
            } else if (result == GameResult::WIN) {
                us.setLost();
                them.setWon();

                const auto color = board_.sideToMove().longStr();
                data_.reason     = color + Match::ADJUDICATION_TB_WIN_MSG;
            } else {
                assert(result == GameResult::LOSE);

                us.setWon();
                them.setLost();

                const auto color = (~board_.sideToMove()).longStr();
                data_.reason     = color + Match::ADJUDICATION_TB_WIN_MSG;
            }

            data_.termination = MatchTermination::ADJUDICATION;

            return true;
        }
    }

    if (config::TournamentConfig->resign.enabled && resign_tracker_.resignable() && us.engine.lastScore() < 0) {
        us.setLost();
        them.setWon();

        const auto color = board_.sideToMove().longStr();

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = color + Match::ADJUDICATION_WIN_MSG;

        return true;
    }

    if (config::TournamentConfig->draw.enabled && draw_tracker_.adjudicatable(board_.fullMoveNumber() - 1)) {
        us.setDraw();
        them.setDraw();

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = Match::ADJUDICATION_MSG;

        return true;
    }

    if (config::TournamentConfig->maxmoves.enabled && maxmoves_tracker_.maxmovesreached()) {
        us.setDraw();
        them.setDraw();

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = Match::ADJUDICATION_MSG;

        return true;
    }

    return false;
}

std::string Match::convertChessReason(const std::string& color, GameResultReason reason) noexcept {
    if (reason == GameResultReason::CHECKMATE) {
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
