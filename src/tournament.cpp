#include <cassert>
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
            std::uniform_int_distribution<uint64_t> maxLines{startIndex % (openingBook.size() - 1),
                                                             openingBook.size() - 1};

            auto randLine = maxLines(Random::generator);
            assert(randLine < openingBook.size());

            return openingBook[randLine];
        }
        else if (matchConfig.opening.order == "sequential")
        {
            assert(startIndex++ % (openingBook.size() - 1) < openingBook.size());

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
    bool timeout = false;

    std::vector<std::string> output;
    output.reserve(30);

    GameResult res;
    DrawAdjTracker drawTracker;
    ResignAdjTracker resignTracker;
    Match match;
    Move move;
    int score, depth;
    int64_t measuredTime;
    std::string scoreString, scoreType;

    Board board;
    board.loadFen(openingFen);

    match.whiteEngine = board.sideToMove == WHITE ? engine1.getConfig() : engine2.getConfig();
    match.blackEngine = board.sideToMove != WHITE ? engine1.getConfig() : engine2.getConfig();

    engine1.sendUciNewGame();
    engine2.sendUciNewGame();

    match.date = saveTimeHeader ? getDateTime("%Y-%m-%d") : "";
    match.startTime = saveTimeHeader ? getDateTime() : "";
    match.board = board;

    std::string positionInput = "position startpos moves";
    std::string bestMove;

    auto timeLeft_1 = engine1.getConfig().tc;
    auto timeLeft_2 = engine2.getConfig().tc;

    auto start = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point t0;
    std::chrono::high_resolution_clock::time_point t1;

    while (true)
    {
        // Check for game over
        res = board.isGameOver();
        if (res != GameResult::NONE)
        {
            break;
        }

        // Engine 1's turn
        // Write new position
        engine1.writeProcess(positionInput);
        engine1.writeProcess(engine1.buildGoInput(board.sideToMove, timeLeft_1));

        // Start measuring time
        t0 = std::chrono::high_resolution_clock::now();

        output = engine1.readProcess("bestmove", timeout, timeLeft_1.time);

        t1 = std::chrono::high_resolution_clock::now();

        // Subtract measured time
        measuredTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        timeLeft_1.time -= measuredTime;

        // Timeout!
        if (timeLeft_1.time < 0)
        {
            res = GameResult(~board.sideToMove);
            std::stringstream ss;
            ss << "engine " << engine1.getConfig().name << " timed out\n";
            std::cout << ss.str();
            break;
        }

        timeLeft_1.time += timeLeft_1.increment;

        // extract last info line
        if (output.size() > 1)
        {
            std::vector<std::string> info = CMD::Options::splitString(output[output.size() - 2], ' ');

            depth = findElement<int>(info, "depth");
            scoreType = findElement<std::string>(info, "score");

            if (scoreType == "cp")
            {
                score = findElement<int>(info, "cp");

                std::stringstream ss;
                ss << (score >= 0 ? '+' : '-');
                ss << std::fixed << std::setprecision(2) << (float(std::abs(score)) / 100);
                scoreString = ss.str();
            }
            else if (scoreType == "mate")
            {
                score = findElement<int>(info, "mate");
                scoreString = (score > 0 ? "+M" : "-M") + std::to_string(std::abs(score));
            }
            else
            {
                score = 0;
                scoreString = "0.00";
            }
        }
        else
        {
            score = 0;
            scoreString = "0.00";
            depth = 0;
        }

        // find bestmove and add it to the position string
        bestMove = findElement<std::string>(CMD::Options::splitString(output.back(), ' '), "bestmove");
        positionInput += " " + bestMove;

        // play move on internal board and store it for later pgn creation
        move = convertUciToMove(bestMove);
        board.makeMove(move);
        match.moves.emplace_back(move, scoreString, depth, measuredTime);

        // Check for game over
        res = board.isGameOver();
        // If game isn't over by other means check adj
        if (res == GameResult::NONE)
        {
            res = CheckAdj(drawTracker, resignTracker);
        }
        if (res != GameResult::NONE)
        {
            break;
        }

        // Engine 2's turn
        // Write new position
        engine2.writeProcess(positionInput);
        engine2.writeProcess(engine2.buildGoInput(board.sideToMove, timeLeft_2));

        // Start measuring time
        t0 = std::chrono::high_resolution_clock::now();

        output = engine2.readProcess("bestmove", timeout, timeLeft_2.time);

        t1 = std::chrono::high_resolution_clock::now();

        // Subtract measured time
        measuredTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        timeLeft_2.time -= measuredTime;

        // Timeout!
        if (timeLeft_2.time < 0)
        {
            res = GameResult(~board.sideToMove);
            std::stringstream ss;
            ss << "engine " << engine2.getConfig().name << " timed out\n";
            std::cout << ss.str();
            break;
        }

        timeLeft_2.time += timeLeft_2.increment;

        // extract last info line
        if (output.size() > 1)
        {
            std::vector<std::string> info = CMD::Options::splitString(output[output.size() - 2], ' ');

            depth = findElement<int>(info, "depth");
            scoreType = findElement<std::string>(info, "score");

            if (scoreType == "cp")
            {
                score = findElement<int>(info, "cp");

                std::stringstream ss;
                ss << (score >= 0 ? '+' : '-');
                ss << std::fixed << std::setprecision(2) << (float(std::abs(score)) / 100);
                scoreString = ss.str();
            }
            else if (scoreType == "mate")
            {
                score = findElement<int>(info, "mate");
                scoreString = (score > 0 ? "+M" : "-M") + std::to_string(std::abs(score));
            }
            else
            {
                score = 0;
                scoreString = "0.00";
            }
        }
        else
        {
            score = 0;
            scoreString = "0.00";
            depth = 0;
        }

        // find bestmove and add it to the position string
        bestMove = findElement<std::string>(CMD::Options::splitString(output.back(), ' '), "bestmove");
        positionInput += " " + bestMove;

        // play move on internal board and store it for later pgn creation
        move = convertUciToMove(bestMove);
        board.makeMove(move);
        match.moves.emplace_back(move, scoreString, depth, measuredTime);
    }

    auto end = std::chrono::high_resolution_clock::now();

    match.round = round;
    match.result = res;
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

    // engine1 always starts first
    engine1.turn = Turn::FIRST;
    engine2.turn = Turn::SECOND;

    int rounds = localMatchConfig.repeat ? 2 : 1;

    for (int i = 0; i < rounds; i++)
    {
        Match match;
        if (engine1.turn == Turn::FIRST)
            match = startMatch(engine1, engine2, i, fen);
        else
            match = startMatch(engine2, engine1, i, fen);

        matches.emplace_back(match);

        std::string positiveEngine = engine1.turn == Turn::FIRST ? engine1.getConfig().name : engine2.getConfig().name;
        std::string negativeEngine = engine1.turn == Turn::FIRST ? engine2.getConfig().name : engine1.getConfig().name;

        // create a pgn from the played match
        PgnBuilder pgn(match, matchConfig);

        // use a stringstream to build the output to avoid data races with cout <<
        std::stringstream ss;
        ss << "Finished " << gameId + i << "/" << localMatchConfig.games * rounds << " " << positiveEngine << " vs "
           << negativeEngine << "\n";
        //    << pgn.getPGN() << "\n";

        std::cout << ss.str();

        engine1.turn = ~engine1.turn;
        engine2.turn = ~engine2.turn;
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

    int wins = 0;
    int draws = 0;
    int losses = 0;

    int gameCount = 0;

    std::ofstream file;
    std::string filename = (matchConfig.opening.file == "" ? "fast-chess" : matchConfig.opening.file) + ".pgn";
    file.open(filename, std::ios::app);

    for (auto &&result : results)
    {
        auto res = result.get();

        for (auto match : res)
        {
            gameCount++;

            PgnBuilder pgn(match, matchConfig);

            pgns.emplace_back(pgn.getPGN());

            file << pgn.getPGN() << std::endl;

            if (match.result == GameResult::WHITE_WIN)
            {
                if (match.whiteEngine.name == configs[0].name)
                    wins++;
                else
                    losses++;
            }
            else if (match.result == GameResult::BLACK_WIN)
            {
                if (match.blackEngine.name == configs[0].name)
                    wins++;
                else
                    losses++;
            }
            else if (match.result == GameResult::DRAW)
            {
                draws++;
            }
            else
            {
                std::cout << "Couldnt obtain Game Result" << std::endl;
            }

            Elo elo(wins, losses, draws);

            std::cout << "Score of " << configs[0].name << " vs " << configs[1].name << ": " << wins << " - " << losses
                      << " - " << draws << " Games: " << gameCount << "\n"
                      << "Elo difference: " << elo.getElo() << std::endl;
        }
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

GameResult Tournament::CheckAdj(DrawAdjTracker drawTracker, ResignAdjTracker resignTracker)
{
    // Check draw adj
    if (matchConfig.draw.enabled)
    {
        if (drawTracker.moveNumber >= matchConfig.draw.moveNumber)
        {
            if (drawTracker.moveCount >= matchConfig.draw.moveCount)
            {
                return GameResult::DRAW;
            }
        }
    }
    // Check Resign adj
    if (matchConfig.resign.enabled)
    {
        if (resignTracker.moveCount >= matchConfig.resign.moveCount)
        {
            return resignTracker.losingSide == WHITE ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
        }
    }
    return GameResult::NONE;
}