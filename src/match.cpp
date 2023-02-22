#include "match.h"

Match::Match(const MatchConfig &mc)
{
    loadConfig(mc);
}

void Match::loadConfig(const MatchConfig &mc)
{
    match_config = mc;
}

void Match::startMatch(std::vector<EngineConfiguration> engines /* Match stuff*/)
{
}
