#include "matchmaking/tournament.hpp"

#include <vector>

#include "matchmaking/match.hpp"
#include "tournament.hpp"

namespace fast_chess
{

Tournament::Tournament(const CMD::GameManagerOptions &game_options)
{
}

void Tournament::loadConfig(const CMD::GameManagerOptions &mc)
{
}

void Tournament::stop()
{
}

void Tournament::printElo()
{
}

void Tournament::startTournament(const std::vector<EngineConfiguration> &engine_configs)
{
}

std::string Tournament::fetchNextFen()
{
    if (opening_book_.size() == 0)
    {
        return startpos_;
    }
    else if (match_config_.opening.format == "pgn")
    {
        // todo: implementation
    }
    else if (match_config_.opening.format == "epd")
    {
        return opening_book_[(match_config_.opening.start + fen_index_++) % opening_book_.size()];
    }

    return startpos_;
}

void Tournament::writeToFile(const std::string &data)
{
    // Acquire the lock
    const std::lock_guard<std::mutex> lock(file_mutex_);

    file_ << data << std::endl;
}

void fast_chess::Tournament::updateStats(const std::string &us, const std::string &them,
                                         const Stats &stats)
{
    const std::unique_lock<std::mutex> lock(results_mutex_);

    results_[us][them] += stats;
}

void Tournament::launchMatch(CMD::GameManagerOptions localMatchConfig,
                             const std::vector<EngineConfiguration> &configs,
                             const std::string &fen)
{
}

} // namespace fast_chess
