#include "matchmaking/output.hpp"

#include "chess/board.hpp"
#include "elo.hpp"
#include "matchmaking/tournament.hpp"
#include "output.hpp"

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

void Fastchess::printElo(Tournament& tournament, std::string first, std::string second) {
    if (tournament.engineCount() != 2) return;

    Stats stats;

    // lock the results
    {
        const std::unique_lock<std::mutex> lock(tournament.results_mutex_);

        if (tournament.results_.find(first) == tournament.results_.end()) std::swap(first, second);

        stats = tournament.results_[first][second];
    }

    int64_t games = stats.sum();

    // clang-format off
    std::stringstream ss;
    ss << "--------------------------------------------------------\n"
       << "Score of " << first << " vs " <<second << " after ";

    if (tournament.gameConfig().report_penta)
    {
       stats.wins = stats.penta_WW * 2 + stats.penta_WD + stats.penta_WL;
       stats.losses = stats.penta_LL * 2 + stats.penta_LD + stats.penta_WL;
       stats.draws = stats.penta_WD + stats.penta_LD + stats.penta_DD * 2;

       ss
       << games / 2 << " rounds: " << stats.wins << " - " << stats.losses << " - " << stats.draws
       << " (" << std::fixed << std::setprecision(2) << (float(stats.wins) + (float(stats.draws) * 0.5)) / games << ")\n"
       << "Ptnml:   "
       << std::right << std::setw(7) << "WW"
       << std::right << std::setw(7) << "WD"
       << std::right << std::setw(7) << "DD/WL"
       << std::right << std::setw(7) << "LD"
       << std::right << std::setw(7) << "LL" << "\n"
       << "Distr:   "
       << std::right << std::setw(7) << stats.penta_WW
       << std::right << std::setw(7) << stats.penta_WD
       << std::right << std::setw(7) << stats.penta_WL + stats.penta_DD
       << std::right << std::setw(7) << stats.penta_LD
       << std::right << std::setw(7) << stats.penta_LL << "\n";
    }  
    else {
        ss << games << " games: " << stats.wins << " - " << stats.losses << " - " << stats.draws
           << "\n";
    }
    // clang-format on

    Elo elo(stats.wins, stats.losses, stats.draws);
    // Elo white_advantage(white_stats.wins, white_stats.losses, stats.draws);

    ss << std::fixed;

    if (sprt_.isValid()) {
        ss << "LLR: " << sprt_.getLLR(stats.wins, stats.draws, stats.losses) << " "
           << sprt_.getBounds() << " " << sprt_.getElo() << "\n";
    }

    ss << std::setprecision(1) << "Stats:  "
       << "W: " << (float(stats.wins) / games) * 100 << "%   "
       << "L: " << (float(stats.losses) / games) * 100 << "%   "
       << "D: " << (float(stats.draws) / games) * 100 << "%   "
       << "TF: " << tournament.timeouts() << "\n";
    // ss << "White advantage: " << white_advantage.getElo() << "\n";
    ss << "Elo difference: " << elo.getElo()
       << "\n--------------------------------------------------------\n";
    std::cout << ss.str();
}
void Fastchess::startMatch(const EngineConfiguration&, const EngineConfiguration&, int, int) {
    return;
}

void Fastchess::endMatch(Tournament& tournament, const MatchData& match_data, int game_id,
                         int round_id) {
    std::stringstream ss;
    // clang-format off
        ss << "Finished game " 
           << game_id + 1 
           << "/" 
           << tournament.gameConfig().games 
           << " in round " 
           << round_id 
           << "/" 
           << tournament.gameConfig().rounds 
           << " total played " 
           << tournament.matchCount() 
           << "/" 
           << tournament.totalCount()
           << " " 
           << match_data.players.first.config.name 
           << " vs "
           << match_data.players.second.config.name 
           << ": " 
           << resultToString(match_data) 
           << "\n";
    // clang-format on

    std::cout << ss.str();
}

}  // namespace fast_chess
