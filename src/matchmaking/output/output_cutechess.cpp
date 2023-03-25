#include "chess/board.hpp"
#include "elo.hpp"
#include "matchmaking/output/output_factory.hpp"
#include "matchmaking/tournament.hpp"

namespace fast_chess {

void Cutechess::printElo(Tournament& tournament, std::string first, std::string second) {
    if (tournament.engineCount() != 2) return;

    Stats stats;

    // lock the results
    {
        const std::unique_lock<std::mutex> lock(tournament.results_mutex_);

        if (tournament.results_.find(first) == tournament.results_.end()) std::swap(first, second);

        stats = tournament.results_[first][second];
    }

    int64_t games = stats.sum();
    Elo elo(stats.wins, stats.losses, stats.draws);

    // clang-format off
    std::stringstream ss;
    ss  << "Score of " 
        << first 
        << " vs " 
        << second 
        << ": "
        << stats.wins 
        << " - " 
        << stats.losses
        << " - " 
        << stats.draws 
        << " "
        << " [] " 
        << games
        << "\n";
    
    ss  << "Elo difference: " 
        << elo.getElo()
        << ", "
        << "LOS: "
        << elo.getLos(stats.wins, stats.losses)
        << ", "
        << "DrawRatio: "
        << elo.getDrawRatio(stats.wins, stats.losses, stats.draws)
        << "\n";

    // clang-format on
    std::cout << ss.str();
}

void Cutechess::startMatch(const EngineConfiguration& engine1_config,
                           const EngineConfiguration& engine2_config, int round_id,
                           int total_count) {
    std::stringstream ss;
    // clang-format off
    ss  << "Started game " 
        << round_id 
        << " of " 
        << total_count
        << " ("
        <<  engine1_config.cmd
        << " vs "
        << engine2_config.cmd
        << ")\n";
    // clang-format on
    std::cout << ss.str();
}

void Cutechess::endMatch(Tournament& tournament, const MatchData& match_data, int, int round_id) {
    Stats stats;
    std::string first = match_data.players.first.config.name;
    std::string second = match_data.players.second.config.name;
    // lock the results
    {
        const std::unique_lock<std::mutex> lock(tournament.results_mutex_);

        if (tournament.results_.find(first) == tournament.results_.end()) std::swap(first, second);

        stats = tournament.results_[first][second];
    }

    std::stringstream ss;
    // clang-format off
    ss  << "Finished game " 
        << round_id 
        << " ("
        << match_data.players.first.config.cmd
        << " vs "
        << match_data.players.second.config.cmd
        << "): "
        << resultToString(match_data)
        << " "
        << "{}"
        << "\n";
    // clang-format on

    // clang-format off
    ss  << "Score of " 
        << match_data.players.first.config.cmd 
        << " vs " << match_data.players.second.config.cmd 
        << ": "
        << stats.wins 
        << " - " 
        << stats.losses 
        << " - " 
        << stats.draws 
        << " "
        << "[] " 
        << stats.sum()
        << "\n";
    // clang-format on

    std::cout << ss.str();
}

}  // namespace fast_chess
