#include "match.h"

Tournament::Tournament(const TournamentConfig &mc)
{
    loadConfig(mc);
}

void Tournament::loadConfig(const MatchConfig &mc)
{
    match_config = mc;
}

GameResult Tournament::startMatch(int i /* Tournament stuff*/)
{
    // Initialize variables
    const int64_t timeoutThreshold = 0;

    Board board;
    board.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    UciEngine engine1, engine2;
    engine1.startEngine("./DummyEngine.exe");
    engine2.startEngine("./DummyEngine.exe");

    GameResult result;
    bool timeout = false;
    std::string positionInput = "position startpos moves";
    std::vector<std::string> output;

    while (true)
    {
        // Check for game over
        result = board.isGameOver();
        if (result != GameResult::NONE)
        {
            return result;
        }

        // Engine 1's turn
        engine1.writeProcess(positionInput);
        engine1.writeProcess("go depth 20");
        output = engine1.readProcess("bestmove", timeout, timeoutThreshold);
        std::string bestMove = findElement<std::string>(splitString(output.back(), ' '), "bestmove");
        positionInput += " " + bestMove;
        board.make_move(convertUciToMove(bestMove));

        // Check for game over
        result = board.isGameOver();
        if (result != GameResult::NONE)
        {
            return result;
        }

        // Engine 2's turn
        engine2.writeProcess(positionInput);
        engine2.writeProcess("go depth 20");
        output = engine2.readProcess("bestmove", timeout, timeoutThreshold);
        bestMove = findElement<std::string>(splitString(output.back(), ' '), "bestmove");
        positionInput += " " + bestMove;
        board.make_move(convertUciToMove(bestMove));
    }
}

void Tournament::startTournament(/* Tournament stuff*/)
{

    std::vector<std::future<GameResult>> results;

    for (int i = 1; i < 2; ++i)
    {
        results.emplace_back(pool.enqueue(startMatch, this, 0));
    }

    for (auto &&result : results)
    {
        auto res = result.get();
        std::cout << int(res) << std::endl;
    }
}
