#include "match.h"

Tournament::Tournament(const TournamentConfig &mc)
{
    loadConfig(mc);
}

void Tournament::loadConfig(const MatchConfig &mc)
{
    match_config = mc;
}

std::vector<std::string> Tournament::startMatch(int i /* Tournament stuff*/)
{
    bool timeout = false;
    UciEngine uci_engine;
    uci_engine.startEngine("./DummyEngine.exe");
    uci_engine.sendUci();
    uci_engine.readUci();
    uci_engine.writeProcess("go depth 20");
    auto res = uci_engine.readProcess("bestmove", timeout);

    return res;
}

void Tournament::startTournament(/* Tournament stuff*/)
{

    std::vector<std::future<std::vector<std::string>>> results;

    for (int i = 1; i < 2; ++i)
    {
        results.emplace_back(pool.enqueue(startMatch, this, 0));
    }

    for (auto &&result : results)
    {
        auto res = result.get();
        for (auto var : res)
        {
            std::cout << var << std::endl;
        }
    }
}
