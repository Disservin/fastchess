#include "match.h"

Match::Match(const MatchConfig &mc)
{
    load_config(mc);
}

void Match::load_config(const MatchConfig &mc)
{
    match_config = mc;
}

void Match::startMatch(std::vector<EngineConfiguration> engines /* Match stuff*/)
{
}
