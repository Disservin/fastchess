#include "matchmaking/match.hpp"
#include "match.hpp"

namespace fast_chess
{

fast_chess::Match::Match(CMD::GameManagerOptions game_config,
                         const EngineConfiguration &engine1_config,
                         const EngineConfiguration &engine2_config)
{
    this->game_config_ = game_config;

    player_1_ = Participant(engine1_config);
}

void Match::playMatch(const std::string &openingFen)
{
    board_.loadFen(openingFen);

    player_1_.engine_.sendUciNewGame();
    player_2_.engine_.sendUciNewGame();

    match_data_.fen = board_.getFen();
    match_data_.date = Logger::getDateTime();
    match_data_.date = Logger::getDateTime("%Y-%m-%d");

    std::string position_input =
        board_.getFen() == startpos_ ? "position startpos" : "position fen " + board_.getFen();

    auto our_time = player_1_.engine_.getConfig().tc;
    auto their_time = player_2_.engine_.getConfig().tc;

    const auto start_time = std::chrono::high_resolution_clock::now();

    while (true)
    {
    }

    const auto end_time = std::chrono::high_resolution_clock::now();

    // match_data_.round = roundId_;
    match_data_.end_time = Logger::getDateTime();
    match_data_.duration = Logger::formatDuration(
        std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time));
}

MatchData Match::getMatchData() const
{
    return match_data_;
}

bool Match::tellEngine(UciEngine &engine, const std::string &input)
{
    engine.writeProcess(input);
    if (!hasErrors(engine))
        return false;

    return true;
}

bool Match::hasErrors(UciEngine &engine)
{
    if (!engine.checkErrors().empty())
    {
        match_data_.needs_restart = game_config_.recover;
        return false;
    }

    return true;
}

bool Match::isResponsive(UciEngine &engine)
{
    // engine's turn
    if (!engine.isResponsive())
    {
        Logger::coutInfo("Warning: Engine", engine.getConfig().name,
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
    return MoveData();
}

bool Match::playNextMove(Participant &player, std::string &position_input,
                         TimeControl &time_left_us, const TimeControl &time_left_them)
{
    std::vector<std::string> output;
    output.reserve(30);

    const int64_t timeout_threshold = time_left_us.fixed_time != 0 ? 0 : time_left_us.time;

    bool timeout = false;

    if (!isResponsive(player.engine_))
        return false;

    // inform the engine about the new position
    if (!tellEngine(player.engine_, position_input))
        return false;

    // Send go command
    if (!tellEngine(player.engine_, (player.engine_.buildGoInput(board_.getSideToMove(),
                                                                 time_left_us, time_left_them))))
        return false;

    // Start measuring time
    const auto t0 = std::chrono::high_resolution_clock::now();

    output = player.engine_.readProcess("bestmove", timeout, timeout_threshold);

    // End of search time
    const auto t1 = std::chrono::high_resolution_clock::now();

    // An error has occured, thus abort match. Throw error if engine recovery is
    // turned off.
    if (!player.engine_.getError().empty())
    {
        match_data_.needs_restart = game_config_.recover;
        Logger::coutInfo("Warning: Engine", player.engine_.getConfig().name, "disconnects #");
        if (!game_config_.recover)
            throw std::runtime_error("Warning: Can't write to engine.");

        return false;
    }

    // Subtract measured time from engine time
    const auto measuredTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    time_left_us.time -= measuredTime;

    if ((time_left_us.fixed_time != 0 &&
         measuredTime - game_config_.overhead > time_left_us.fixed_time) ||
        (time_left_us.fixed_time == 0 && time_left_us.time + game_config_.overhead < 0))
    {
        player.getInfo().result = "0-1";
        player.getInfo().score = GameResult::LOSE;

        match_data_.termination = "timeout";
        Logger::coutInfo("Warning: Engine", player.engine_.getConfig().name, "loses on time #");
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
    match_data_.moves.emplace_back(parseEngineOutput(output, bestMove, measuredTime));

    if (!match_data_.legal)
    {
        // The move was not legal
        player.getInfo().result = "0-1";
        player.getInfo().score = GameResult::LOSE;

        match_data_.termination = "illegal move";

        Logger::coutInfo("Warning: Engine", player.engine_.getConfig().name,
                         "played an illegal move:", bestMove, "#");
        return false;
    }

    // Check for game over
    player.getInfo().score = board_.isGameOver();
    if (player.getInfo().score != GameResult::NONE)
        return false;

    return true;
}
} // namespace fast_chess
