#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "elo.h"
#include "pgn_builder.h"
#include "rand.h"
#include "tournament.h"

Tournament::Tournament(const CMD::GameManagerOptions &mc)
{
    loadConfig(mc);
}

void Tournament::loadConfig(const CMD::GameManagerOptions &mc)
{
    matchConfig = mc;

    if (matchConfig.opening.file != "")
    {
        std::ifstream openingFile;
        std::string line;
        openingFile.open(matchConfig.opening.file);

        while (std::getline(openingFile, line))
        {
            openingBook.emplace_back(line);
        }

        openingFile.close();
    }
}

std::string Tournament::fetchNextFen()
{
    if (openingBook.size() == 0)
    {
        return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    }
    else if (matchConfig.opening.format == "pgn")
    {
        // todo: implementation
    }
    else if (matchConfig.opening.format == "epd")
    {
        if (matchConfig.opening.order == "random")
        {
            std::uniform_int_distribution<unsigned long> maxLines{startIndex % (openingBook.size() - 1),
                                                                  openingBook.size() - 1};

            auto randLine = maxLines(Random::generator);
            assert(randLine >= 0 && randLine < openingBook.size());

            return openingBook[randLine];
        }
        else if (matchConfig.opening.order == "sequential")
        {
            assert(startIndex++ % (openingBook.size() - 1) >= 0 &&
                   startIndex++ % (openingBook.size() - 1) < openingBook.size());

            return openingBook[startIndex++ % (openingBook.size() - 1)];
        }
    }

    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

std::vector<std::string> Tournament::getPGNS() const
{
    return pgns;
}

Match Tournament::startMatch(UciEngine &engine1, UciEngine &engine2, int round, std::string openingFen)
{
    Board board;
    board.loadFen(openingFen);

    std::string winner;
    GameResult res;

    Match match;
    Move move;

    const int64_t timeoutThreshold = 0;
    bool timeout = false;

    engine1.sendUciNewGame();
    engine2.sendUciNewGame();

    match.date = saveTimeHeader ? getDateTime("%Y-%m-%d") : "";
    match.startTime = saveTimeHeader ? getDateTime() : "";
    match.board = board;

    std::vector<std::string> output;
    output.reserve(30);

    std::string positionInput = "position startpos moves";

    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        // Check for game over
        res = board.isGameOver();
        if (res != GameResult::NONE)
        {
            if (res == GameResult::DRAW)
            {
                winner = "draw";
            }
            else
            {
                winner = Color(res) == engine1.color ? engine1.getConfig().name : engine2.getConfig().name;
            }
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
        res = board.isGameOver();
        if (res != GameResult::NONE)
        {
            if (res == GameResult::DRAW)
            {
                winner = "draw";
            }
            else
            {
                winner = Color(res) == engine1.color ? engine1.getConfig().name : engine2.getConfig().name;
            }
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

    match.round = round;
    match.result = res;
    match.winner = winner;
    match.endTime = saveTimeHeader ? getDateTime() : "";
    match.duration =
        saveTimeHeader ? formatDuration(std::chrono::duration_cast<std::chrono::seconds>(end - start)) : "";

    return match;
}

std::vector<Match> Tournament::runH2H(CMD::GameManagerOptions localMatchConfig,
                                      std::vector<EngineConfiguration> configs, int gameId, std::string fen)
{
    // Initialize variables
    std::vector<Match> matches;

    UciEngine engine1, engine2;
    engine1.loadConfig(configs[0]);
    engine2.loadConfig(configs[1]);

    engine1.startEngine();
    engine2.startEngine();

    engine1.color = WHITE;
    engine2.color = BLACK;

    int rounds = localMatchConfig.repeat ? 2 : 1;

    for (int i = 0; i < rounds; i++)
    {
        matches.emplace_back(startMatch(engine1, engine2, i, fen));

        std::stringstream ss;

        std::string whiteEngineName = engine1.color == WHITE ? engine1.getConfig().name : engine2.getConfig().name;
        std::string blackEngineName = engine1.color == WHITE ? engine2.getConfig().name : engine1.getConfig().name;

        ss << "Finished " << gameId + i << "/" << localMatchConfig.games * rounds << " " << whiteEngineName << " vs "
           << blackEngineName << "\n";

        std::cout << ss.str();

        engine1.color = ~engine1.color;
        engine2.color = ~engine2.color;
    }

    return matches;
}

void Tournament::startTournament(std::vector<EngineConfiguration> configs)
{
    pgns.clear();
    pool.resize(matchConfig.concurrency);

    std::vector<std::future<std::vector<Match>>> results;

    int rounds = matchConfig.repeat ? 2 : 1;

    for (int i = 1; i <= matchConfig.games * rounds; i += rounds)
    {
        results.emplace_back(
            pool.enqueue(std::bind(&Tournament::runH2H, this, matchConfig, configs, i, fetchNextFen())));
    }

    int i = 1;

    int wins = 0;
    int draws = 0;
    int losses = 0;

    for (auto &&result : results)
    {
        auto res = result.get();

        // std::cout << "Finished " << i << "/" << matchConfig.games << std::endl;

        for (auto match : res)
        {
            PgnBuilder pgn(match, matchConfig);
            pgns.emplace_back(pgn.getPGN());

            if (match.winner == configs[0].name)
            {
                wins++;
            }
            else if (match.winner == configs[1].name)
            {
                losses++;
            }
            else
            {
                draws++;
            }
            Elo elo(wins, draws, losses);

            std::cout << "Wins: " << wins << " Draws: " << draws << " Losses: " << losses << "\n"
                      << "Elo diff: " << elo.diff() << std::endl;
        }

        i++;
    }
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