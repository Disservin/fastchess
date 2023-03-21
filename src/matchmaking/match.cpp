#include "matchmaking/match.hpp"
#include "match.hpp"

namespace fast_chess
{

Match::Match(const CMD::GameManagerOptions &game_config, const EngineConfiguration &engine1_config,
             const EngineConfiguration &engine2_config)
    : player_1_(Participant(engine1_config)), player_2_(Participant(engine2_config)),
      drawTracker_(DrawAdjTracker(game_config.draw.score, 0)),
      resignTracker_(ResignAdjTracker(game_config.resign.score, 0))
{
    this->game_config_ = game_config;
}

void Match::playMatch(const std::string &openingFen)
{
    board_.loadFen(openingFen);

    player_1_.sendUciNewGame();
    player_2_.sendUciNewGame();

    std::string position_input =
        board_.getFen() == startpos_ ? "position startpos" : "position fen " + board_.getFen();

    player_1_.info_.color = board_.getSideToMove();
    player_2_.info_.color = ~board_.getSideToMove();

    auto first_player_time = player_1_.getConfig().tc;
    auto second_player_time = player_2_.getConfig().tc;

    match_data_.fen = board_.getFen();

#ifndef TESTS
    match_data_.start_time = Logger::getDateTime();
    match_data_.date = Logger::getDateTime("%Y-%m-%d");
    const auto start_time = std::chrono::high_resolution_clock::now();
#endif // TESTS

    while (true)
    {
        if (!playNextMove(player_1_, player_2_, position_input, first_player_time,
                          second_player_time))
            break;

        if (!playNextMove(player_2_, player_1_, position_input, second_player_time,
                          first_player_time))
            break;
    }

// match_data_.round = roundId_;
#ifndef TESTS
    const auto end_time = std::chrono::high_resolution_clock::now();

    match_data_.end_time = Logger::getDateTime();
    match_data_.duration = Logger::formatDuration(
        std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time));
#endif
}

MatchData Match::getMatchData()
{
    match_data_.players = std::make_pair(player_1_.info_, player_2_.info_);
    return match_data_;
}

void Match::updateTrackers(const Score moveScore, const int move_number)
{
    // Score is low for draw adj, increase the counter
    if (move_number >= game_config_.draw.move_number && abs(moveScore) < drawTracker_.draw_score)
        drawTracker_.move_count++;
    // Score wasn't low enough for draw adj, since we care about consecutive
    // moves we have to reset the counter
    else
        drawTracker_.move_count = 0;
    // Score is low for resign adj, increase the counter (this purposely makes
    // it possible that a move can work for both draw and resign adj for
    // whatever reason that might be the case)
    if (abs(moveScore) > resignTracker_.resign_score)
        resignTracker_.move_count++;
    else
        resignTracker_.move_count = 0;
}

GameResult Match::checkAdj(const Score score)

{
    // Check draw adj
    if (game_config_.draw.enabled && drawTracker_.move_count >= game_config_.draw.move_count)
    {
        match_data_.termination = "adjudication";
        return GameResult::DRAW;
    }

    // Check Resign adj
    if (game_config_.resign.enabled &&
        resignTracker_.move_count >= game_config_.resign.move_count && score != mate_score_)
    {
        match_data_.termination = "adjudication";

        // We have the Score for the last side that moved, if it's bad that side
        // is the resigning one.
        return score < resignTracker_.resign_score ? GameResult::LOSE : GameResult::WIN;
    }

    return GameResult::NONE;
}

bool Match::tellEngine(Participant &player, const std::string &input)
{
    player.writeProcess(input);
    if (!hasErrors(player))
        return false;

    return true;
}

bool Match::hasErrors(Participant &player)
{
    if (!player.checkErrors().empty())
    {
        match_data_.needs_restart = game_config_.recover;
        return false;
    }

    return true;
}

bool Match::isResponsive(Participant &player)
{
    // engine's turn
    if (!player.isResponsive())
    {
        Logger::coutInfo("Warning: Engine", player.getConfig().name,
                         "disconnects. It was not responsive.");

        if (!game_config_.recover)
        {
            throw std::runtime_error("Warning: Engine not responsive");
        }

        return false;
    }
    return true;
}

MoveData Match::parseEngineOutput(const std::vector<std::string> &output, const std::string &move,
                                  int64_t measured_time)
{
    auto move_data = MoveData(move, "0.00", measured_time, 0, 0, 0, 0);

    if (output.size() <= 1)
        return move_data;

    // extract last info line
    const auto info = CMD::splitString(output[output.size() - 2], ' ');

    // Missing elements default to 0
    std::string scoreType = CMD::findElement<std::string>(info, "score").value_or("cp");
    move_data.depth = CMD::findElement<int>(info, "depth").value_or(0);
    move_data.seldepth = CMD::findElement<int>(info, "seldepth").value_or(0);
    move_data.nodes = CMD::findElement<uint64_t>(info, "nodes").value_or(0);

    if (scoreType == "cp")
    {
        move_data.score = CMD::findElement<int>(info, "cp").value_or(0);

        std::stringstream ss;
        ss << (move_data.score >= 0 ? '+' : '-');
        ss << std::fixed << std::setprecision(2) << (float(std::abs(move_data.score)) / 100);
        move_data.score_string = ss.str();
    }
    else if (scoreType == "mate")
    {
        move_data.score = CMD::findElement<int>(info, "mate").value_or(0);
        move_data.score_string =
            (move_data.score > 0 ? "+M" : "-M") + std::to_string(std::abs(move_data.score));
        move_data.score = mate_score_;
    }

    // verify pv
    for (const auto &info : output)
    {
        const auto tokens = CMD::splitString(info, ' ');

        if (!CMD::contains(tokens, "moves"))
            continue;

        auto tmp = board_;
        auto it = std::find(tokens.begin(), tokens.end(), "pv");
        while (++it != tokens.end())
        {
            if (!tmp.makeMove(convertUciToMove(tmp, *it)))
            {
                std::stringstream ss;
                ss << "Warning: Illegal pv move " << *it << ".\n";
                std::cout << ss.str();
                break;
            };
        }
    }

    return move_data;
}

bool Match::playNextMove(Participant &player, Participant &enemy, std::string &position_input,
                         TimeControl &time_left_us, const TimeControl &time_left_them)
{
    std::vector<std::string> output;
    output.reserve(30);

    const int64_t timeout_threshold = time_left_us.fixed_time != 0 ? 0 : time_left_us.time;

    bool timeout = false;

    if (!isResponsive(player))
        return false;

    // inform the engine about the new position
    if (!tellEngine(player, position_input))
        return false;

    // Send go command
    if (!tellEngine(player,
                    (player.buildGoInput(board_.getSideToMove(), time_left_us, time_left_them))))
        return false;

    // Start measuring time
    const auto t0 = std::chrono::high_resolution_clock::now();

    output = player.readProcess("bestmove", timeout, timeout_threshold);

    // End of search time
    const auto t1 = std::chrono::high_resolution_clock::now();

    // An error has occured, thus abort match. Throw error if engine recovery is
    // turned off.
    if (!player.getError().empty())
    {
        match_data_.needs_restart = game_config_.recover;
        Logger::coutInfo("Warning: Engine", player.getConfig().name, "disconnects #");
        if (!game_config_.recover)
            throw std::runtime_error("Warning: Can't write to engine.");

        return false;
    }

    // Subtract measured time from engine time
    const auto measured_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    time_left_us.time -= measured_time;

    if ((time_left_us.fixed_time != 0 &&
         measured_time - game_config_.overhead > time_left_us.fixed_time) ||
        (time_left_us.fixed_time == 0 && time_left_us.time + game_config_.overhead < 0))
    {
        player.info_.score = GameResult::LOSE;
        enemy.info_.score = GameResult::WIN;

        match_data_.termination = "timeout";
        Logger::coutInfo("Warning: Engine", player.getConfig().name, "loses on time #");
        return false;
    }

    // Now we add increment!
    time_left_us.time += time_left_us.increment;

    // find bestmove and add it to the position string
    const auto bestMove =
        CMD::findElement<std::string>(CMD::splitString(output.back(), ' '), "bestmove").value();

    // is this the first move? If so we need to insert "moves".
    if (match_data_.moves.size() == 0)
        position_input += " moves";

    position_input += " " + bestMove;

    // play move on internal board_ and store it for later pgn creation
    match_data_.legal = board_.makeMove(convertUciToMove(board_, bestMove));
    match_data_.moves.emplace_back(parseEngineOutput(output, bestMove, measured_time));

    if (!match_data_.legal)
    {
        // The move was not legal
        player.info_.score = GameResult::LOSE;
        enemy.info_.score = GameResult::WIN;

        match_data_.termination = "illegal move";

        Logger::coutInfo("Warning: Engine", player.getConfig().name,
                         "played an illegal move:", bestMove, "#");
        return false;
    }

    // Check for game over
    auto res = board_.isGameOver();

    if (res == GameResult::LOSE)
    {
        player.info_.score = ~res;
        // enemy lost
        enemy.info_.score = res;

        return false;
    }
    else if (res == GameResult::DRAW)
    {
        player.info_.score = GameResult::DRAW;
        enemy.info_.score = GameResult::DRAW;

        return false;
    }

    updateTrackers(match_data_.moves.back().score, match_data_.moves.size());
    res = checkAdj(match_data_.moves.back().score);

    if (res == GameResult::DRAW)
    {
        player.info_.score = GameResult::DRAW;
        enemy.info_.score = GameResult::DRAW;
    }
    else if (res != GameResult::NONE)
    {
        player.info_.score = res;
        enemy.info_.score = ~res;
    }

    return true;
}

} // namespace fast_chess
