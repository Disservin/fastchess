#include <matchmaking/match/match.hpp>

#include <algorithm>

#include <types/tournament_options.hpp>
#include <util/date.hpp>
#include <util/helper.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

using namespace std::literals;

namespace chrono = std::chrono;
using clock      = chrono::high_resolution_clock;

using namespace chess;

void Match::addMoveData(const Participant& player, int64_t measured_time_ms) {
    MoveData move_data = MoveData(player.engine.bestmove(), "0.00", measured_time_ms, 0, 0, 0, 0);

    if (player.engine.output().size() <= 1) {
        data_.moves.push_back(move_data);
        return;
    }

    // extract last info line
    const auto score_type = player.engine.lastScoreType();
    const auto info       = player.engine.lastInfo();

    move_data.nps      = str_utils::findElement<int>(info, "nps").value_or(0);
    move_data.depth    = str_utils::findElement<int>(info, "depth").value_or(0);
    move_data.seldepth = str_utils::findElement<int>(info, "seldepth").value_or(0);
    move_data.nodes    = str_utils::findElement<uint64_t>(info, "nodes").value_or(0);
    move_data.score    = player.engine.lastScore();

    // Missing elements default to 0
    std::stringstream ss;

    if (score_type == ScoreType::CP) {
        ss << (move_data.score >= 0 ? '+' : '-');
        ss << std::fixed << std::setprecision(2) << (float(std::abs(move_data.score)) / 100);
    } else if (score_type == ScoreType::MATE) {
        ss << (move_data.score > 0 ? "+M" : "-M") << std::to_string(std::abs(move_data.score));
    } else {
        ss << "ERR";
    }

    move_data.score_string = ss.str();

    verifyPvLines(player);

    data_.moves.push_back(move_data);
}

void Match::prepare() {
    board_.set960(tournament_options_.variant == VariantType::FRC);
    board_.setFen(opening_.fen);

    start_position_ = board_.getFen() == chess::constants::STARTPOS ? "startpos" : board_.getFen();

    const auto insert_move = [&](const auto& opening_move) {
        const auto move = uci::moveToUci(opening_move, board_.chess960());
        board_.makeMove(opening_move);

        return MoveData(move, "0.00", 0, 0, 0, 0, 0);
    };

    data_ = MatchData(opening_.fen);

    std::transform(opening_.moves.begin(), opening_.moves.end(), std::back_inserter(data_.moves),
                   insert_move);

    draw_tracker_   = DrawTacker(tournament_options_);
    resign_tracker_ = ResignTracker(tournament_options_);
}

void Match::start(UciEngine& engine1, UciEngine& engine2, const std::vector<int>& cpus) {
    prepare();

    Participant player_1 = Participant(engine1);
    Participant player_2 = Participant(engine2);

    player_1.color = board_.sideToMove();
    player_2.color = ~board_.sideToMove();

    player_1.engine.setCpus(cpus);
    player_2.engine.setCpus(cpus);

    player_1.engine.refreshUci();
    player_2.engine.refreshUci();

    const auto start = clock::now();

    try {
        while (true) {
            if (atomic::stop.load()) {
                data_.termination = MatchTermination::INTERRUPT;
                break;
            }

            if (!playMove(player_1, player_2)) break;

            if (atomic::stop.load()) {
                data_.termination = MatchTermination::INTERRUPT;
                break;
            }

            if (!playMove(player_2, player_1)) break;
        }
    } catch (const std::exception& e) {
        if (tournament_options_.recover)
            data_.needs_restart = true;
        else {
            throw e;
        }
    }

    const auto end = clock::now();

    data_.end_time = time::datetime("%Y-%m-%dT%H:%M:%S %z");
    data_.duration = time::duration(chrono::duration_cast<chrono::seconds>(end - start));

    data_.players =
        std::make_pair(MatchData::PlayerInfo{engine1.getConfig(), player_1.result, player_1.color},
                       MatchData::PlayerInfo{engine2.getConfig(), player_2.result, player_2.color});
}

bool Match::playMove(Participant& us, Participant& opponent) {
    const auto gameover = board_.isGameOver();
    const auto name     = us.engine.getConfig().name;

    if (gameover.second == GameResult::DRAW) {
        setDraw(us, opponent);
    }

    if (gameover.second == GameResult::LOSE) {
        setLose(us, opponent);
    }

    if (gameover.first != GameResultReason::NONE) {
        data_.reason = convertChessReason(name, gameover.first);
        return false;
    }

    // disconnect
    if (!us.engine.isResponsive()) {
        setLose(us, opponent);

        data_.termination = MatchTermination::DISCONNECT;
        data_.reason      = name + Match::DISCONNECT_MSG;

        return false;
    }

    // write new uci position
    std::vector<std::string> uci_moves;
    std::transform(data_.moves.begin(), data_.moves.end(), std::back_inserter(uci_moves),
                   [](const MoveData& data) { return data.move; });

    us.engine.writeEngine(Participant::buildPositionInput(uci_moves, start_position_));
    // write go command
    us.engine.writeEngine(us.buildGoInput(board_.sideToMove(), opponent.getTimeControl()));

    // wait for bestmove
    const auto t0 = clock::now();
    us.engine.readEngine("bestmove", us.getTimeoutThreshold());
    const auto t1 = clock::now();

    if (atomic::stop) {
        data_.termination = MatchTermination::INTERRUPT;

        return false;
    }

    // Time forfeit
    const auto elapsed_millis = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
    if (!us.updateTime(elapsed_millis)) {
        setLose(us, opponent);

        data_.termination = MatchTermination::TIMEOUT;
        data_.reason      = name + Match::TIMEOUT_MSG;

        Logger::log<Logger::Level::WARN>("Warning; Engine", name, "loses on time");

        return false;
    }

    draw_tracker_.update(us.engine.lastScore(), data_.moves.size(), us.engine.lastScoreType());
    resign_tracker_.update(us.engine.lastScore(), us.engine.lastScoreType());

    addMoveData(us, elapsed_millis);

    const auto best_move = us.engine.bestmove();
    const auto move      = uci::uciToMove(board_, best_move);

    if (!isLegal(move)) {
        setLose(us, opponent);

        data_.termination = MatchTermination::ILLEGAL_MOVE;
        data_.reason      = name + Match::ILLEGAL_MSG;

        Logger::log<Logger::Level::WARN>("Warning; Illegal move", best_move, "played by", name);

        return false;
    }

    board_.makeMove(move);

    return !adjudicate(us, opponent);
}

bool Match::isLegal(Move move) const noexcept {
    Movelist moves;
    movegen::legalmoves(moves, board_);

    return moves.find(move) > -1;
}

void Match::verifyPvLines(const Participant& us) {
    const auto verifyPv = [&](const std::vector<std::string>& tokens, std::string_view info) {
        auto tmp = board_;
        auto it  = std::find(tokens.begin(), tokens.end(), "pv") + 1;

        Movelist moves;

        std::for_each(it, tokens.end(), [&](const auto& token) {
            movegen::legalmoves(moves, tmp);

            if (moves.find(uci::uciToMove(tmp, token)) == -1) {
                Logger::log<Logger::Level::WARN>("Warning; Illegal pv move ", token, "pv:", info);
            }

            tmp.makeMove(uci::uciToMove(tmp, token));
        });
    };

    for (const auto& info : us.engine.output()) {
        const auto tokens = str_utils::splitString(info, ' ');

        // skip lines without pv
        if (!str_utils::contains(tokens, "pv")) continue;

        verifyPv(tokens, info);
    }
}

void Match::setDraw(Participant& us, Participant& them) noexcept {
    us.result   = GameResult::DRAW;
    them.result = GameResult::DRAW;
}

void Match::setWin(Participant& us, Participant& them) noexcept {
    us.result   = GameResult::WIN;
    them.result = GameResult::LOSE;
}

void Match::setLose(Participant& us, Participant& them) noexcept {
    us.result   = GameResult::LOSE;
    them.result = GameResult::WIN;
}

bool Match::adjudicate(Participant& us, Participant& them) noexcept {
    if (tournament_options_.draw.enabled && draw_tracker_.adjudicatable()) {
        setDraw(us, them);

        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = Match::ADJUDICATION_MSG;

        return true;
    }

    if (tournament_options_.resign.enabled && resign_tracker_.resignable()) {
        data_.termination = MatchTermination::ADJUDICATION;
        data_.reason      = us.engine.getConfig().name;

        if (us.engine.lastScore() < tournament_options_.resign.score) {
            setLose(us, them);
            data_.reason += Match::ADJUDICATION_LOSE_MSG;
        } else {
            setWin(us, them);
            data_.reason += Match::ADJUDICATION_WIN_MSG;
        }

        return true;
    }

    return false;
}

std::string Match::convertChessReason(const std::string& engine_name,
                                      GameResultReason reason) noexcept {
    if (reason == GameResultReason::CHECKMATE) {
        return engine_name + Match::CHECKMATE_MSG;
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

}  // namespace fast_chess