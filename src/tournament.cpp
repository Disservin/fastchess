#include <chrono>
#include <iomanip>
#include <iostream>

#include "pgn_builder.h"
#include "tournament.h"

Tournament::Tournament(const CMD::GameManagerOptions &mc)
{
    loadConfig(mc);
}

void Tournament::loadConfig(const CMD::GameManagerOptions &mc)
{
    matchConfig = mc;
}

std::string Tournament::fetchNextFen() const
{
    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

std::vector<Match> Tournament::startMatch(std::vector<EngineConfiguration> configs, std::string openingFen)
{
    // Initialize variables
    std::vector<Match> matches;

    const int64_t timeoutThreshold = 0;

    UciEngine engine1, engine2;
    engine1.loadConfig(configs[0]);
    engine2.loadConfig(configs[1]);

    engine1.startEngine();
    engine2.startEngine();

    engine1.color = WHITE;
    engine2.color = BLACK;

    std::array<GameResult, 2> result;
    std::vector<std::string> output;

    bool timeout = false;

    int rounds = matchConfig.repeat ? 2 : 1;

    Board board;
    Move move;

    for (int i = 0; i < rounds; i++)
    {

        engine1.sendUciNewGame();
        engine2.sendUciNewGame();

        board.loadFen(openingFen);

        auto start = std::chrono::high_resolution_clock::now();

        Match match;

        match.date = getDateTime("%Y-%m-%d");
        match.startTime = getDateTime();
        match.board = board;

        std::string positionInput = "position startpos moves";

        while (true)
        {
            // Check for game over
            result[i] = board.isGameOver();
            if (result[i] != GameResult::NONE)
            {
                break;
            }

            // Engine 1's turn
            engine1.writeProcess(positionInput);
            engine1.writeProcess(engine1.buildGoInput());
            output = engine1.readProcess("bestmove", timeout, timeoutThreshold);

            std::string bestMove = findElement<std::string>(splitString(output.back(), ' '), "bestmove");
            positionInput += " " + bestMove;

            move = convertUciToMove(bestMove);
            board.makeMove(move);
            match.moves.emplace_back(move);

            // Check for game over
            result[i] = board.isGameOver();
            if (result[i] != GameResult::NONE)
            {
                break;
            }

            // Engine 2's turn
            engine2.writeProcess(positionInput);
            engine2.writeProcess(engine2.buildGoInput());
            output = engine2.readProcess("bestmove", timeout, timeoutThreshold);

            bestMove = findElement<std::string>(splitString(output.back(), ' '), "bestmove");
            positionInput += " " + bestMove;

            move = convertUciToMove(bestMove);
            board.makeMove(move);
            match.moves.emplace_back(move);
        }

        auto end = std::chrono::high_resolution_clock::now();

        match.round = i;
        match.result = result[i];
        match.endTime = getDateTime();
        match.duration = formatDuration(std::chrono::duration_cast<std::chrono::seconds>(end - start));
        matches.emplace_back(match);

        engine1.color = ~engine1.color;
        engine2.color = ~engine2.color;
    }

    return matches;
}

void Tournament::startTournament(std::vector<EngineConfiguration> configs)
{
    pool.resize(matchConfig.concurrency);

    std::vector<std::future<std::vector<Match>>> results;

    for (int i = 0; i < matchConfig.games; ++i)
    {
        results.emplace_back(pool.enqueue(std::bind(&Tournament::startMatch, this, configs, fetchNextFen())));
    }

    int i = 1;

    auto start = std::chrono::high_resolution_clock::now();

    for (auto &&result : results)
    {
        auto res = result.get();

        std::cout << "Finished " << i << "/" << matchConfig.games << std::endl;

        PgnBuilder pgn(res, matchConfig);

        i++;
    }

    auto now = std::chrono::high_resolution_clock::now();

    std::cout << "finished in " << std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() << "ms"
              << std::endl;
}

std::string Tournament::getDateTime(std::string format)
{
    // Get the current time in UTC
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), format.c_str());
    return ss.str();
}

std::string Tournament::formatDuration(std::chrono::seconds duration)
{
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = duration;

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours.count() << ":" << std::setfill('0') << std::setw(2)
       << minutes.count() << ":" << std::setfill('0') << std::setw(2) << seconds.count();
    return ss.str();
}