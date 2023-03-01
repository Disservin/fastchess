#include <cassert>
#include <chrono>
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
        return STARTPOS;
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

    return STARTPOS;
}

std::vector<std::string> Tournament::getPGNS() const
{
    return pgns;
}

void Tournament::setStorePGN(bool v)
{
    storePGNS = v;
}

void Tournament::printElo()
{
    Elo elo(wins, losses, draws);

    std::stringstream ss;
    // clang-format off
           
    ss << "---------------------------\n" 
       << "Result of " << engineNames[0] << " vs " << engineNames[1] 
       << ": " << wins << " - " << losses << " - " << draws 
       << " (" << std::fixed << std::setprecision(2) << (float(wins) + (float(draws) * 0.5)) / totalCount << ")\n"
       << "Ptnml:   " 
       << std::right << std::setw(7) << "WW" 
       << std::right << std::setw(7) << "WD" 
       << std::right << std::setw(7) << "DD/WL" 
       << std::right << std::setw(7) << "LD" 
       << std::right << std::setw(7) << "LL" << "\n"
       << "         " 
       << std::right << std::setw(7) << pentaWW 
       << std::right << std::setw(7) << pentaWD 
       << std::right << std::setw(7) << pentaWL 
       << std::right << std::setw(7) << pentaLD 
       << std::right << std::setw(7) << pentaLL << "\n";
    // clang-format on

    if (sprt.isValid())
    {
        ss << "LLR: " << sprt.getLLR(wins, draws, losses) << " " << sprt.getBounds() << "\n";
    }

    ss << "Elo difference: " << elo.getElo() << "\n---------------------------\n";
    std::cout << ss.str();
}

void Tournament::writeToFile(const std::string &data)
{
    // Acquire the lock
    const std::lock_guard<std::mutex> lock(fileMutex);

    file << data << std::endl;
}

void Tournament::checkEngineStatus(UciEngine &engine, Match &match, int &retflag, int roundId)
{
    if (engine.checkErrors(roundId) != "")
    {
        match.needsRestart = matchConfig.recover;
        retflag = 2;
    }
}

void Tournament::playNextMove(UciEngine &engine, std::string &positionInput, Board &board, TimeControl &timeLeftUs,
                              TimeControl &timeLeftThem, GameResult &res, Match &match, DrawAdjTracker &drawTracker,
                              ResignAdjTracker &resignTracker, int &retflag, int roundId)
{
    std::vector<std::string> output;
    output.reserve(30);

    bool timeout = false;

    retflag = 1;

    // engine's turn
    if (!engine.isResponsive())
    {
        std::stringstream ss;
        ss << "\nEngine " << engine.getConfig().name << " was not responsive.\n";
        std::cout << ss.str();
        if (!matchConfig.recover)
        {
            throw std::runtime_error(ss.str());
        }
        return;
    }

    // Write new position
    engine.writeProcess(positionInput);
    checkEngineStatus(engine, match, retflag, roundId);

    engine.writeProcess(engine.buildGoInput(board.sideToMove, timeLeftUs, timeLeftThem));
    checkEngineStatus(engine, match, retflag, roundId);

    // Start measuring time
    auto t0 = std::chrono::high_resolution_clock::now();

    output = engine.readProcess("bestmove", timeout, timeLeftUs.time);

    auto t1 = std::chrono::high_resolution_clock::now();

    if (engine.getError() != "")
    {
        match.needsRestart = matchConfig.recover;
        retflag = 2;
        std::stringstream ss;
        ss << engine.getError() << "\nCant write to engine " << engine.getConfig().name << " #" << roundId << "\n";
        if (!matchConfig.recover)
        {
            throw std::runtime_error(ss.str());
        }
    }

    // Subtract measured time
    auto measuredTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    timeLeftUs.time -= measuredTime;

    // Timeout!
    if (timeLeftUs.time < 0)
    {
        retflag = 2;
        res = GameResult(~board.sideToMove);
        match.termination = "timeout";

        std::stringstream ss;
        ss << "engine " << engine.getConfig().name << " timed out #" << roundId << "\n";
        std::cout << ss.str();

        return;
    }

    timeLeftUs.time += timeLeftUs.increment;

    // find bestmove and add it to the position string
    const auto bestMove = findElement<std::string>(CMD::Options::splitString(output.back(), ' '), "bestmove");
    positionInput += " " + bestMove;

    // play move on internal board and store it for later pgn creation
    match.legal = board.makeMove(convertUciToMove(bestMove));
    match.moves.emplace_back(parseEngineOutput(output, bestMove, measuredTime));

    if (!match.legal)
    {
        retflag = 2;
        res = GameResult(~board.sideToMove);
        match.termination = "illegal move";

        std::stringstream ss;
        ss << "engine " << engine.getConfig().name << " played an illegal move: " << bestMove << " #" << roundId
           << "\n";

        std::cout << ss.str();

        return;
    }

    // Update Trackers
    updateTrackers(drawTracker, resignTracker, match.moves.back().score);

    // Check for game over
    res = board.isGameOver();

    // If game isn't over by other means check adj
    if (res == GameResult::NONE)
    {
        res = checkAdj(match, drawTracker, resignTracker, match.moves.back().score, ~board.sideToMove);
    }

    if (res != GameResult::NONE)
    {
        retflag = 2;
        return;
    }
}

Match Tournament::startMatch(UciEngine &engine1, UciEngine &engine2, int roundId, std::string openingFen)
{
    GameResult res = GameResult::NONE;
    ResignAdjTracker resignTracker = {};
    DrawAdjTracker drawTracker = {};
    Match match = {};

    Board board = {};
    board.loadFen(openingFen);

    int retflag = 0;

    match.whiteEngine = board.sideToMove == WHITE ? engine1.getConfig() : engine2.getConfig();
    match.blackEngine = board.sideToMove != WHITE ? engine1.getConfig() : engine2.getConfig();

    engine1.sendUciNewGame();
    engine2.sendUciNewGame();

    checkEngineStatus(engine1, match, retflag, roundId);
    checkEngineStatus(engine2, match, retflag, roundId);

    match.date = saveTimeHeader ? getDateTime("%Y-%m-%d") : "";
    match.startTime = saveTimeHeader ? getDateTime() : "";
    match.board = board;

    std::string positionInput =
        openingFen == STARTPOS ? "position startpos moves" : "position fen " + openingFen + " moves";

    auto timeLeft_1 = engine1.getConfig().tc;
    auto timeLeft_2 = engine2.getConfig().tc;

    auto start = std::chrono::high_resolution_clock::now();

    while (!pool.stop)
    {

        playNextMove(engine1, positionInput, board, timeLeft_1, timeLeft_2, res, match, drawTracker, resignTracker,
                     retflag, roundId);

        if (retflag == 2)
            break;

        playNextMove(engine2, positionInput, board, timeLeft_2, timeLeft_1, res, match, drawTracker, resignTracker,
                     retflag, roundId);

        if (retflag == 2)
            break;
    }

    auto end = std::chrono::high_resolution_clock::now();

    match.round = roundId;
    match.result = res;
    match.endTime = saveTimeHeader ? getDateTime() : "";
    match.duration =
        saveTimeHeader ? formatDuration(std::chrono::duration_cast<std::chrono::seconds>(end - start)) : "";

    return match;
}

std::vector<Match> Tournament::runH2H(CMD::GameManagerOptions localMatchConfig,
                                      std::vector<EngineConfiguration> configs, int roundId, std::string fen)
{
    // Initialize variables
    std::vector<Match> matches;

    UciEngine engine1, engine2;
    engine1.loadConfig(configs[0]);
    engine1.resetError();
    engine1.startEngine();

    engine2.loadConfig(configs[1]);
    engine2.resetError();
    engine2.startEngine();

    engine1.checkErrors(roundId);
    engine2.checkErrors(roundId);

    // engine1 always starts first
    engine1.turn = Turn::FIRST;
    engine2.turn = Turn::SECOND;

    int games = localMatchConfig.repeat ? 2 : localMatchConfig.games;

    int localWins = 0;
    int localLosses = 0;
    int localDraws = 0;

    for (int i = 0; i < games; i++)
    {
        Match match;
        if (engine1.turn == Turn::FIRST)
            match = startMatch(engine1, engine2, roundId, fen);
        else
            match = startMatch(engine2, engine1, roundId, fen);

        if (match.needsRestart)
        {
            i--;
            engine1.restartEngine();
            engine2.restartEngine();
            continue;
        }

        matches.emplace_back(match);

        std::string positiveEngine = engine1.turn == Turn::FIRST ? engine1.getConfig().name : engine2.getConfig().name;
        std::string negativeEngine = engine1.turn == Turn::FIRST ? engine2.getConfig().name : engine1.getConfig().name;

        // use a stringstream to build the output to avoid data races with cout <<
        std::stringstream ss;
        ss << "Finished game " << i + 1 << "/" << games << " in round " << roundId << "/" << localMatchConfig.rounds
           << " total played " << totalCount << "/" << localMatchConfig.rounds * games << " " << positiveEngine
           << " vs " << negativeEngine << ": " << resultToString(match.result) << "\n";

        std::cout << ss.str();

        engine1.turn = ~engine1.turn;
        engine2.turn = ~engine2.turn;

        if (match.result == GameResult::WHITE_WIN)
        {
            if (match.whiteEngine.name == configs[0].name)
                localWins++;
            else
                localLosses++;
        }
        else if (match.result == GameResult::BLACK_WIN)
        {
            if (match.blackEngine.name == configs[0].name)
                localWins++;
            else
                localLosses++;
        }
        else if (match.result == GameResult::DRAW)
        {
            localDraws++;
        }
        else
        {
            std::cout << "Couldnt obtain Game Result\n";
        }

        totalCount++;
    }

    if (localWins == 2)
        pentaWW++;

    if (localWins == 1 && localDraws == 1)
        pentaWD++;

    if ((localWins == 1 && localLosses == 1) || localDraws == 2)
        pentaWL++;

    if (localLosses == 1 && localDraws == 1)
        pentaLD++;

    if (localLosses == 2)
        pentaLL++;

    wins += localWins;
    losses += localLosses;
    draws += localDraws;

    roundCount++;

    if (roundCount % localMatchConfig.ratinginterval == 0)
        printElo();

    // Write matches to file
    for (const auto &match : matches)
    {
        PgnBuilder pgn(match, matchConfig);

        writeToFile(pgn.getPGN());
    }

    return matches;
}

void Tournament::startTournament(std::vector<EngineConfiguration> configs)
{
    if (configs.size() < 2)
    {
        throw std::runtime_error("Need at least two engines to start!");
    }

    pgns.clear();
    pool.resize(matchConfig.concurrency);

    std::vector<std::future<std::vector<Match>>> results;

    std::string filename = (matchConfig.pgn.file == "" ? "fast-chess" : matchConfig.pgn.file) + ".pgn";
    file.open(filename, std::ios::app);

    for (int i = 1; i <= matchConfig.rounds; i++)
    {
        results.emplace_back(
            pool.enqueue(std::bind(&Tournament::runH2H, this, matchConfig, configs, i, fetchNextFen())));
    }

    engineNames.push_back(configs[0].name);
    engineNames.push_back(configs[1].name);

    sprt = SPRT(matchConfig.sprt.alpha, matchConfig.sprt.beta, matchConfig.sprt.elo0, matchConfig.sprt.elo1);

    while (sprt.isValid() && !pool.stop)
    {
        double llr = sprt.getLLR(wins, draws, losses);
        if (sprt.getResult(llr) != SPRT_CONTINUE)
        {
            pool.kill();
            printElo();

            return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }

    if (storePGNS)
    {
        for (auto &&result : results)
        {
            auto res = result.get();

            for (const Match &match : res)
            {
                PgnBuilder pgn(match, matchConfig);
                pgns.emplace_back(pgn.getPGN());
            }
        }
    }

    while (!pool.stop && roundCount < matchConfig.rounds)
    {
        // prevent accessive atomic checks
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }

    printElo();
}

void Tournament::stopPool()
{
    pool.kill();
}

MoveData Tournament::parseEngineOutput(const std::vector<std::string> &output, const std::string &move,
                                       int64_t measuredTime)
{
    std::string scoreString = "";
    std::string scoreType;
    int score = 0;
    int depth = 0;
    uint64_t nodes = 0;
    // extract last info line
    if (output.size() > 1)
    {
        std::vector<std::string> info = CMD::Options::splitString(output[output.size() - 2], ' ');

        depth = findElement<int>(info, "depth");
        scoreType = findElement<std::string>(info, "score");
        nodes = findElement<uint64_t>(info, "nodes");

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
            score = MATE_SCORE;
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

    return MoveData(move, scoreString, measuredTime, depth, score, nodes);
}

std::string Tournament::getDateTime(std::string format)
{
    // Get the current time in UTC
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm buf;

#ifdef _WIN32
    auto res = gmtime_s(&buf, &time_t_now);
    if (res != 0)
    {
        throw std::runtime_error("gmtime_s failed");
    }

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(&buf, format.c_str());
    return ss.str();
#else
    auto res = gmtime_r(&time_t_now, &buf);

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(res, format.c_str());
    return ss.str();
#endif
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

void Tournament::updateTrackers(DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker, const Score moveScore)
{
    // Score is low for draw adj, increase the counter
    if (abs(moveScore) < drawTracker.drawScore)
    {
        drawTracker.moveCount++;
    }
    // Score wasn't low enough for draw adj, since we care about consecutive moves we have to reset the counter
    else
    {
        drawTracker.moveCount = 0;
    }

    // Score is low for resign adj, increase the counter (this purposely makes it possible that a move can work for both
    // draw and resign adj for whatever reason that might be the case)
    if (abs(moveScore) > resignTracker.resignScore)
    {
        resignTracker.moveCount++;
    }
    else
    {
        resignTracker.moveCount = 0;
    }
}

GameResult Tournament::checkAdj(Match &match, const DrawAdjTracker drawTracker, const ResignAdjTracker resignTracker,
                                const Score score, const Color lastSideThatMoved)

{
    const int moveNumber = match.moves.size() / 2;

    // Check draw adj
    if (matchConfig.draw.enabled && moveNumber >= matchConfig.draw.moveNumber &&
        drawTracker.moveCount >= matchConfig.draw.moveCount)
    {
        match.termination = "adjudication";
        return GameResult::DRAW;
    }

    // Check Resign adj
    if (matchConfig.resign.enabled && resignTracker.moveCount >= matchConfig.resign.moveCount && score != MATE_SCORE)
    {
        match.termination = "adjudication";

        // We have the Score for the last side that moved, if it's bad that side is the resigning one so give the other
        // side the win.
        return score < resignTracker.resignScore ? GameResult(~lastSideThatMoved) : GameResult(lastSideThatMoved);
    }

    return GameResult::NONE;
}