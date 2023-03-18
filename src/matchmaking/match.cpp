#include "matchmaking/match.hpp"

namespace fast_chess
{

Match::Match(CMD::GameManagerOptions match_config, const EngineConfiguration &engine1_config,
             const EngineConfiguration &engine2_config, bool save_time_header)
{
    match_config_ = match_config;

    engine1_.loadConfig(engine1_config);
    engine1_.resetError();
    engine1_.startEngine();

    engine2_.loadConfig(engine2_config);
    engine2_.resetError();
    engine2_.startEngine();

    engine1_.checkErrors();
    engine2_.checkErrors();

    // engine1_ always starts first
    engine1_.turn = Turn::FIRST;
    engine2_.turn = Turn::SECOND;

    resignTracker_ = ResignAdjTracker(match_config_.resign.score, 0);
    drawTracker_ = DrawAdjTracker(match_config_.draw.score, 0);

    save_time_header_ = save_time_header;
}

MatchInfo Match::startMatch(int roundId, std::string openingFen)
{
    roundId_ = roundId;

    board_.loadFen(openingFen);

    mi_.white_engine =
        board_.getSideToMove() == WHITE ? engine1_.getConfig() : engine2_.getConfig();
    mi_.black_engine =
        board_.getSideToMove() != WHITE ? engine1_.getConfig() : engine2_.getConfig();

    engine1_.sendUciNewGame();
    engine2_.sendUciNewGame();

    mi_.fen = board_.getFen();
    mi_.start_time = save_time_header_ ? Logger::getDateTime() : "";
    mi_.date = save_time_header_ ? Logger::getDateTime("%Y-%m-%d") : "";

    std::string positionInput =
        board_.getFen() == startpos_ ? "position startpos" : "position fen " + board_.getFen();

    auto timeLeft_1 = engine1_.getConfig().tc;
    auto timeLeft_2 = engine2_.getConfig().tc;

    const auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        if (!playNextMove(engine1_, positionInput, timeLeft_1, timeLeft_2))
            break;

        if (!playNextMove(engine2_, positionInput, timeLeft_2, timeLeft_1))
            break;
    }

    const auto end = std::chrono::high_resolution_clock::now();

    mi_.round = roundId_;
    mi_.end_time = save_time_header_ ? Logger::getDateTime() : "";
    mi_.duration =
        save_time_header_
            ? Logger::formatDuration(std::chrono::duration_cast<std::chrono::seconds>(end - start))
            : "";

    return mi_;
}

MoveData Match::parseEngineOutput(const std::vector<std::string> &output, const std::string &move,
                                  int64_t measuredTime)
{
    std::string score_string = "0.00";
    uint64_t nodes = 0;
    int score = 0;
    int depth = 0;
    int selDepth = 0;

    // extract last info line
    if (output.size() > 1)
    {
        const auto info = CMD::Options::splitString(output[output.size() - 2], ' ');
        // Missing elements default to 0
        std::string scoreType = findElement<std::string>(info, "score").value_or("cp");
        depth = findElement<int>(info, "depth").value_or(0);
        selDepth = findElement<int>(info, "seldepth").value_or(0);
        nodes = findElement<uint64_t>(info, "nodes").value_or(0);

        if (scoreType == "cp")
        {
            score = findElement<int>(info, "cp").value_or(0);

            std::stringstream ss;
            ss << (score >= 0 ? '+' : '-');
            ss << std::fixed << std::setprecision(2) << (float(std::abs(score)) / 100);
            score_string = ss.str();
        }
        else if (scoreType == "mate")
        {
            score = findElement<int>(info, "mate").value_or(0);
            score_string = (score > 0 ? "+M" : "-M") + std::to_string(std::abs(score));
            score = mate_score_;
        }
    }

    // verify pv
    for (const auto &info : output)
    {
        auto tmp = board_;
        const auto tokens = CMD::Options::splitString(info, ' ');

        if (!CMD::Options::contains(tokens, "moves"))
            continue;

        std::size_t index = std::find(tokens.begin(), tokens.end(), "pv") - tokens.begin();
        index++;
        for (; index < tokens.size(); index++)
        {
            if (!tmp.makeMove(convertUciToMove(tmp, tokens[index])))
            {
                std::stringstream ss;
                ss << "Warning: Illegal pv move " << tokens[index] << ".\n";
                std::cout << ss.str();
                break;
            };
        }
    }

    return MoveData(move, score_string, measuredTime, depth, selDepth, score, nodes);
}

void Match::updateTrackers(const Score moveScore, const int move_number)
{
    // Score is low for draw adj, increase the counter
    if (move_number >= match_config_.draw.move_number && abs(moveScore) < drawTracker_.draw_score)
    {
        drawTracker_.move_count++;
    }
    // Score wasn't low enough for draw adj, since we care about consecutive
    // moves we have to reset the counter
    else
    {
        drawTracker_.move_count = 0;
    }

    // Score is low for resign adj, increase the counter (this purposely makes
    // it possible that a move can work for both draw and resign adj for
    // whatever reason that might be the case)
    if (abs(moveScore) > resignTracker_.resign_score)
    {
        resignTracker_.move_count++;
    }
    else
    {
        resignTracker_.move_count = 0;
    }
}

GameResult Match::checkAdj(const Score score, const Color lastSideThatMoved)

{
    // Check draw adj
    if (match_config_.draw.enabled && drawTracker_.move_count >= match_config_.draw.move_count)
    {
        mi_.termination = "adjudication";
        return GameResult::DRAW;
    }

    // Check Resign adj
    if (match_config_.resign.enabled &&
        resignTracker_.move_count >= match_config_.resign.move_count && score != mate_score_)
    {
        mi_.termination = "adjudication";

        // We have the Score for the last side that moved, if it's bad that side
        // is the resigning one so give the other side the win.
        return score < resignTracker_.resign_score ? GameResult(~lastSideThatMoved)
                                                   : GameResult(lastSideThatMoved);
    }

    return GameResult::NONE;
}

bool Match::checkEngineStatus(UciEngine &engine)
{
    if (!engine.checkErrors(roundId_).empty())
    {
        mi_.needs_restart = match_config_.recover;
        return false;
    }
    return true;
}

bool Match::playNextMove(UciEngine &engine, std::string &positionInput, TimeControl &timeLeftUs,
                         const TimeControl &timeLeftThem)
{
    std::vector<std::string> output;
    output.reserve(30);

    bool timeout = false;

    // engine's turn
    if (!engine.isResponsive())
    {
        Logger::coutInfo("Warning: Engine", engine.getConfig().name,
                         "disconnects. It was not responsive.");

        if (!match_config_.recover)
        {
            throw std::runtime_error("Warning: Engine not responsive");
        }

        return false;
    }

    // Write new position
    engine.writeProcess(positionInput);
    if (!checkEngineStatus(engine))
        return false;

    // Send uci go command to engine
    engine.writeProcess(engine.buildGoInput(board_.getSideToMove(), timeLeftUs, timeLeftThem));
    if (!checkEngineStatus(engine))
        return false;

    // Start measuring time
    const auto t0 = std::chrono::high_resolution_clock::now();

    output =
        engine.readProcess("bestmove", timeout, timeLeftUs.fixed_time != 0 ? 0 : timeLeftUs.time);

    const auto t1 = std::chrono::high_resolution_clock::now();

    if (!engine.getError().empty())
    {
        mi_.needs_restart = match_config_.recover;
        Logger::coutInfo("Warning: Engine", engine.getConfig().name, "disconnects #", roundId_);

        if (!match_config_.recover)
        {
            throw std::runtime_error("Warning: Can't write to engine.");
        }

        return false;
    }

    // Subtract measured time
    const auto measuredTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    timeLeftUs.time -= measuredTime;

    if ((timeLeftUs.fixed_time != 0 &&
         measuredTime - match_config_.overhead > timeLeftUs.fixed_time) ||
        (timeLeftUs.fixed_time == 0 && timeLeftUs.time + match_config_.overhead < 0))
    {
        mi_.result = GameResult(~board_.getSideToMove());
        mi_.termination = "timeout";
        Logger::coutInfo("Warning: Engine", engine.getConfig().name, "loses on time #", roundId_);
        return false;
    }

    timeLeftUs.time += timeLeftUs.increment;

    // find bestmove and add it to the position string
    const auto bestMove =
        findElement<std::string>(CMD::Options::splitString(output.back(), ' '), "bestmove").value();

    if (mi_.moves.size() == 0)
        positionInput += " moves";
    positionInput += " " + bestMove;

    // play move on internal board_ and store it for later pgn creation
    mi_.legal = board_.makeMove(convertUciToMove(board_, bestMove));
    mi_.moves.emplace_back(parseEngineOutput(output, bestMove, measuredTime));

    if (!mi_.legal)
    {
        mi_.result = GameResult(~board_.getSideToMove());
        mi_.termination = "illegal move";

        Logger::coutInfo("Warning: Engine", engine.getConfig().name,
                         "played an illegal move:", bestMove, "#", roundId_);
        return false;
    }

    // Update Trackers
    updateTrackers(mi_.moves.back().score, mi_.moves.size());

    // Check for game over
    if ((mi_.result = board_.isGameOver()) != GameResult::NONE ||
        (mi_.result = checkAdj(mi_.moves.back().score, ~board_.getSideToMove())) !=
            GameResult::NONE)
        return false;

    return true;
}

} // namespace fast_chess