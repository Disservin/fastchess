#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "elo.hpp"
#include "pgn_builder.hpp"
#include "rand.hpp"
#include "third_party/mersenne-twister.h"
#include "tournament.hpp"

Tournament::Tournament(const CMD::GameManagerOptions &mc)
{
    const std::string filename = (mc.pgn.file.empty() ? "fast-chess" : mc.pgn.file) + ".pgn";

    file.open(filename, std::ios::app);

    loadConfig(mc);
}

void Tournament::loadConfig(const CMD::GameManagerOptions &mc)
{
    seed(matchConfig.seed);

    matchConfig = mc;

    if (!matchConfig.opening.file.empty())
    {
        std::ifstream openingFile;
        std::string line;
        openingFile.open(matchConfig.opening.file);

        while (std::getline(openingFile, line))
        {
            openingBook.emplace_back(line);
        }

        openingFile.close();

        if (matchConfig.opening.order == "random")
        {
            // Fisher-Yates / Knuth shuffle
            for (size_t i = 0; i <= openingBook.size() - 2; i++)
            {
                size_t j = i + (rand_u32() % (openingBook.size() - i));
                std::swap(openingBook[i], openingBook[j]);
            }
        }
    }

    pool.resize(matchConfig.concurrency);

    sprt = SPRT(matchConfig.sprt.alpha, matchConfig.sprt.beta, matchConfig.sprt.elo0,
                matchConfig.sprt.elo1);
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
        return openingBook[(matchConfig.opening.start + fenIndex++) % openingBook.size()];
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
       << " (" << std::fixed << std::setprecision(2) << (float(wins) + (float(draws) * 0.5)) / (roundCount * this->matchConfig.games) << ")\n"
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
    ss << "Games:" << roundCount * 2 << std::setprecision(1)
       << " W:" << (float(wins) / (roundCount * this->matchConfig.games)) * 100 << "% "
       << "L:" << (float(losses) / (roundCount * this->matchConfig.games)) * 100 << "% "
       << "D:" << (float(draws) / (roundCount * this->matchConfig.games)) * 100 << "%\n";
    ss << "Elo difference: " << elo.getElo() << "\n---------------------------\n";
    std::cout << ss.str();
}

void Tournament::writeToFile(const std::string &data)
{
    // Acquire the lock
    const std::lock_guard<std::mutex> lock(fileMutex);

    file << data << std::endl;
}

bool Tournament::checkEngineStatus(UciEngine &engine, Match &match, int roundId) const
{
    if (!engine.checkErrors(roundId).empty())
    {
        match.needsRestart = matchConfig.recover;
        return false;
    }
    return true;
}

bool Tournament::playNextMove(UciEngine &engine, std::string &positionInput, Board &board,
                              TimeControl &timeLeftUs, const TimeControl &timeLeftThem,
                              GameResult &res, Match &match, DrawAdjTracker &drawTracker,
                              ResignAdjTracker &resignTracker, int roundId)
{
    std::vector<std::string> output;
    output.reserve(30);

    bool timeout = false;

    // engine's turn
    if (!engine.isResponsive())
    {
        Logger::coutInfo("Engine", engine.getConfig().name, "was not responsive.");

        if (!matchConfig.recover)
        {
            throw std::runtime_error("Engine not responsive");
        }

        return false;
    }

    // Write new position
    engine.writeProcess(positionInput);
    if (!checkEngineStatus(engine, match, roundId))
        return false;

    engine.writeProcess(engine.buildGoInput(board.sideToMove, timeLeftUs, timeLeftThem));
    if (!checkEngineStatus(engine, match, roundId))
        return false;

    // Start measuring time
    const auto t0 = std::chrono::high_resolution_clock::now();

    output =
        engine.readProcess("bestmove", timeout, timeLeftUs.fixed_time != 0 ? 0 : timeLeftUs.time);

    const auto t1 = std::chrono::high_resolution_clock::now();

    if (!engine.getError().empty())
    {
        match.needsRestart = matchConfig.recover;
        Logger::coutInfo("Can't write to engine", engine.getConfig().name, "#", roundId);

        if (!matchConfig.recover)
        {
            throw std::runtime_error("Can't write to engine.");
        }

        return false;
    }

    // Subtract measured time
    const auto measuredTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    timeLeftUs.time -= measuredTime;

    if ((timeLeftUs.fixed_time != 0 &&
         measuredTime - matchConfig.overhead > timeLeftUs.fixed_time) ||
        (timeLeftUs.fixed_time == 0 && timeLeftUs.time + matchConfig.overhead < 0))
    {
        res = GameResult(~board.sideToMove);
        match.termination = "timeout";
        Logger::coutInfo("Engine", engine.getConfig().name, "timed out #", roundId);
        return false;
    }

    timeLeftUs.time += timeLeftUs.increment;

    // find bestmove and add it to the position string
    const auto bestMove =
        findElement<std::string>(CMD::Options::splitString(output.back(), ' '), "bestmove");

    if (match.moves.size() == 0)
        positionInput += " moves";
    positionInput += " " + bestMove;

    // play move on internal board and store it for later pgn creation
    match.legal = board.makeMove(convertUciToMove(board, bestMove));
    match.moves.emplace_back(parseEngineOutput(board, output, bestMove, measuredTime));

    if (!match.legal)
    {
        res = GameResult(~board.sideToMove);
        match.termination = "illegal move";

        Logger::coutInfo("Engine", engine.getConfig().name, "played an illegal move:", bestMove,
                         "#", roundId);
        return false;
    }

    // Update Trackers
    updateTrackers(drawTracker, resignTracker, match.moves.back().score, match.moves.size());

    // Check for game over
    res = board.isGameOver();

    // If game isn't over by other means check adj
    if (res == GameResult::NONE)
    {
        res = checkAdj(match, drawTracker, resignTracker, match.moves.back().score,
                       ~board.sideToMove);
    }

    if (res != GameResult::NONE)
    {
        return false;
    }

    return true;
}

Match Tournament::startMatch(UciEngine &engine1, UciEngine &engine2, int roundId,
                             std::string openingFen)
{
    ResignAdjTracker resignTracker = ResignAdjTracker(matchConfig.resign.score, 0);
    DrawAdjTracker drawTracker = DrawAdjTracker(matchConfig.draw.score, 0);
    GameResult res = GameResult::NONE;
    Match match = {};

    Board board = {};
    board.loadFen(openingFen);

    match.whiteEngine = board.sideToMove == WHITE ? engine1.getConfig() : engine2.getConfig();
    match.blackEngine = board.sideToMove != WHITE ? engine1.getConfig() : engine2.getConfig();

    engine1.sendUciNewGame();
    engine2.sendUciNewGame();

    match.fen = board.getFen();
    match.startTime = saveTimeHeader ? Logger::getDateTime() : "";
    match.date = saveTimeHeader ? Logger::getDateTime("%Y-%m-%d") : "";

    std::string positionInput =
        board.getFen() == STARTPOS ? "position startpos" : "position fen " + board.getFen();

    auto timeLeft_1 = engine1.getConfig().tc;
    auto timeLeft_2 = engine2.getConfig().tc;

    const auto start = std::chrono::high_resolution_clock::now();

    while (!pool.stop)
    {
        if (!playNextMove(engine1, positionInput, board, timeLeft_1, timeLeft_2, res, match,
                          drawTracker, resignTracker, roundId))
            break;

        if (!playNextMove(engine2, positionInput, board, timeLeft_2, timeLeft_1, res, match,
                          drawTracker, resignTracker, roundId))
            break;
    }

    const auto end = std::chrono::high_resolution_clock::now();

    match.round = roundId;
    match.result = res;
    match.endTime = saveTimeHeader ? Logger::getDateTime() : "";
    match.duration =
        saveTimeHeader
            ? Logger::formatDuration(std::chrono::duration_cast<std::chrono::seconds>(end - start))
            : "";

    return match;
}

std::vector<Match> Tournament::runH2H(CMD::GameManagerOptions localMatchConfig,
                                      const std::vector<EngineConfiguration> &configs, int roundId,
                                      const std::string &fen)
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

    const int games = localMatchConfig.games;

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

        totalCount++;
        matches.emplace_back(match);

        const std::string positiveEngine =
            engine1.turn == Turn::FIRST ? engine1.getConfig().name : engine2.getConfig().name;
        const std::string negativeEngine =
            engine1.turn == Turn::FIRST ? engine2.getConfig().name : engine1.getConfig().name;

        engine1.turn = ~engine1.turn;
        engine2.turn = ~engine2.turn;

        if (match.result == GameResult::WHITE_WIN)
        {
            localWins += match.whiteEngine.name == configs[0].name ? 1 : 0;
            localLosses += match.whiteEngine.name == configs[0].name ? 0 : 1;
        }
        else if (match.result == GameResult::BLACK_WIN)
        {
            localWins += match.blackEngine.name == configs[0].name ? 1 : 0;
            localLosses += match.blackEngine.name == configs[0].name ? 0 : 1;
        }
        else if (match.result == GameResult::DRAW)
        {
            localDraws++;
        }
        else
        {
            std::cout << "Couldn't obtain Game Result\n";
        }

        std::stringstream ss;
        ss << "Finished game " << i + 1 << "/" << games << " in round " << roundId << "/"
           << localMatchConfig.rounds << " total played " << totalCount << "/"
           << localMatchConfig.rounds * games << " " << positiveEngine << " vs " << negativeEngine
           << ": " << resultToString(match.result) << "\n";

        std::cout << ss.str();
    }

    pentaWW += localWins == 2 ? 1 : 0;
    pentaWD += localWins == 1 && localDraws == 1 ? 1 : 0;
    pentaWL += (localWins == 1 && localLosses == 1) || localDraws == 2 ? 1 : 0;
    pentaLD += localLosses == 1 && localDraws == 1 ? 1 : 0;
    pentaLL += localLosses == 2 ? 1 : 0;

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

void Tournament::startTournament(const std::vector<EngineConfiguration> &configs)
{
    if (configs.size() < 2)
    {
        throw std::runtime_error("Need at least two engines to start!");
    }

    std::vector<std::future<std::vector<Match>>> results;

    for (int i = 1; i <= matchConfig.rounds; i++)
    {
        results.emplace_back(pool.enqueue(
            std::bind(&Tournament::runH2H, this, matchConfig, configs, i, fetchNextFen())));
    }

    engineNames.push_back(configs[0].name);
    engineNames.push_back(configs[1].name);

    while (sprt.isValid() && !pool.stop)
    {
        const double llr = sprt.getLLR(wins, draws, losses);
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
            const auto res = result.get();

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

MoveData Tournament::parseEngineOutput(const Board &board, const std::vector<std::string> &output,
                                       const std::string &move, int64_t measuredTime)
{
    std::string scoreString = "0.00";
    uint64_t nodes = 0;
    int score = 0;
    int depth = 0;

    // extract last info line
    if (output.size() > 1)
    {
        const auto info = CMD::Options::splitString(output[output.size() - 2], ' ');

        std::string scoreType = findElement<std::string>(info, "score");
        depth = findElement<int>(info, "depth");
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
    }

    // verify pv
    for (const auto &info : output)
    {
        auto tmp = board;
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

    return MoveData(move, scoreString, measuredTime, depth, score, nodes);
}

void Tournament::updateTrackers(DrawAdjTracker &drawTracker, ResignAdjTracker &resignTracker,
                                const Score moveScore, const int moveNumber)
{
    // Score is low for draw adj, increase the counter
    if (moveNumber >= matchConfig.draw.moveNumber && abs(moveScore) < drawTracker.drawScore)
    {
        drawTracker.moveCount++;
    }
    // Score wasn't low enough for draw adj, since we care about consecutive
    // moves we have to reset the counter
    else
    {
        drawTracker.moveCount = 0;
    }

    // Score is low for resign adj, increase the counter (this purposely makes
    // it possible that a move can work for both draw and resign adj for
    // whatever reason that might be the case)
    if (abs(moveScore) > resignTracker.resignScore)
    {
        resignTracker.moveCount++;
    }
    else
    {
        resignTracker.moveCount = 0;
    }
}

GameResult Tournament::checkAdj(Match &match, const DrawAdjTracker &drawTracker,
                                const ResignAdjTracker &resignTracker, const Score score,
                                const Color lastSideThatMoved) const

{
    // Check draw adj
    if (matchConfig.draw.enabled && drawTracker.moveCount >= matchConfig.draw.moveCount)
    {
        match.termination = "adjudication";
        return GameResult::DRAW;
    }

    // Check Resign adj
    if (matchConfig.resign.enabled && resignTracker.moveCount >= matchConfig.resign.moveCount &&
        score != MATE_SCORE)
    {
        match.termination = "adjudication";

        // We have the Score for the last side that moved, if it's bad that side
        // is the resigning one so give the other side the win.
        return score < resignTracker.resignScore ? GameResult(~lastSideThatMoved)
                                                 : GameResult(lastSideThatMoved);
    }

    return GameResult::NONE;
}
