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

std::array<GameResult, 2> Tournament::startMatch(std::vector<EngineConfiguration> configs, std::string openingFen)
{
    // Initialize variables
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

    for (int i = 0; i < rounds; i++)
    {
        engine1.sendUciNewGame();
        engine2.sendUciNewGame();

        board.loadFen(openingFen);

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
            board.makeMove(convertUciToMove(bestMove));

            // Check for game over
            result[i] = board.isGameOver();
            if (result[i] != GameResult::NONE)
            {
                break;
            }

            // Engine 2's turn
            engine2.writeProcess(positionInput);
            engine2.writeProcess(engine1.buildGoInput());
            output = engine2.readProcess("bestmove", timeout, timeoutThreshold);

            bestMove = findElement<std::string>(splitString(output.back(), ' '), "bestmove");
            positionInput += " " + bestMove;
            board.makeMove(convertUciToMove(bestMove));
        }

        engine1.color = ~engine1.color;
        engine2.color = ~engine2.color;
    }

    return result;
}

void Tournament::startTournament(std::vector<EngineConfiguration> configs)
{
    pool.resize(matchConfig.concurrency);

    std::vector<std::future<std::array<GameResult, 2>>> results;

    for (int i = 0; i < matchConfig.games; ++i)
    {
        results.emplace_back(pool.enqueue(startMatch, this, configs, fetchNextFen()));
    }

    int i = 1;
    for (auto &&result : results)
    {
        auto res = result.get();

        std::cout << "Finished " << i << "/" << matchConfig.games << std::endl;
        std::cout << int(res[0]) << " " << int(res[1]) << std::endl;

        i++;
    }
}
