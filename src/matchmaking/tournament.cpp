#include "matchmaking/tournament.hpp"

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "elo.hpp"
#include "pgn_builder.hpp"
#include "rand.hpp"

namespace fast_chess
{

Tournament::Tournament(const CMD::GameManagerOptions &mc)
{
    const std::string filename = (mc.pgn.file.empty() ? "fast-chess" : mc.pgn.file) + ".pgn";

    file_.open(filename, std::ios::app);

    loadConfig(mc);
}

void Tournament::loadConfig(const CMD::GameManagerOptions &mc)
{
    Random::mersenne_rand.seed(match_config_.seed);

    match_config_ = mc;

    if (!match_config_.opening.file.empty())
    {
        std::ifstream openingFile;
        std::string line;
        openingFile.open(match_config_.opening.file);

        while (std::getline(openingFile, line))
        {
            opening_book_.emplace_back(line);
        }

        openingFile.close();

        if (match_config_.opening.order == "random")
        {
            // Fisher-Yates / Knuth shuffle
            for (size_t i = 0; i <= opening_book_.size() - 2; i++)
            {
                size_t j = i + (Random::mersenne_rand() % (opening_book_.size() - i));
                std::swap(opening_book_[i], opening_book_[j]);
            }
        }
    }

    pool_.resize(match_config_.concurrency);

    sprt_ = SPRT(match_config_.sprt.alpha, match_config_.sprt.beta, match_config_.sprt.elo0,
                 match_config_.sprt.elo1);
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

std::vector<std::string> Tournament::getPGNS() const
{
    return pgns_;
}

void Tournament::setStorePGN(bool v)
{
    store_pgns_ = v;
    save_time_header_ = !v;
}

void Tournament::printElo() const
{
    Elo elo(wins_, losses_, draws_);
    Elo white_advantage(white_wins_, white_losses_, draws_);
    std::stringstream ss;

    // clang-format off
    ss << "--------------------------------------------------------\n"
       << "Score of " << engine_names_[0] << " vs " << engine_names_[1] << " after " << round_count_ * match_config_.games << " games: "
       << wins_ << " - " << losses_ << " - " << draws_
       << " (" << std::fixed << std::setprecision(2) << (float(wins_) + (float(draws_) * 0.5)) / (round_count_ * match_config_.games) << ")\n"
       << "Ptnml:   "
       << std::right << std::setw(7) << "WW"
       << std::right << std::setw(7) << "WD"
       << std::right << std::setw(7) << "DD/WL"
       << std::right << std::setw(7) << "LD"
       << std::right << std::setw(7) << "LL" << "\n"
       << "Distr:   "
       << std::right << std::setw(7) << penta_WW_
       << std::right << std::setw(7) << penta_WD_
       << std::right << std::setw(7) << penta_WL_
       << std::right << std::setw(7) << penta_LD_
       << std::right << std::setw(7) << penta_LL_ << "\n";
    // clang-format on

    if (sprt_.isValid())
    {
        ss << "LLR: " << sprt_.getLLR(wins_, draws_, losses_) << " " << sprt_.getBounds() << " "
           << sprt_.getElo() << "\n";
    }
    ss << std::setprecision(1) << "Stats:  "
       << "W: " << (float(wins_) / (round_count_ * match_config_.games)) * 100 << "%   "
       << "L: " << (float(losses_) / (round_count_ * match_config_.games)) * 100 << "%   "
       << "D: " << (float(draws_) / (round_count_ * match_config_.games)) * 100 << "%   "
       << "TF: " << timeouts_ << "\n";
    ss << "White advantage: " << white_advantage.getElo() << "\n";
    ss << "Elo difference: " << elo.getElo()
       << "\n--------------------------------------------------------\n";
    std::cout << ss.str();
}

void Tournament::writeToFile(const std::string &data)
{
    // Acquire the lock
    const std::lock_guard<std::mutex> lock(file_mutex_);

    file_ << data << std::endl;
}

std::vector<MatchInfo> Tournament::runH2H(CMD::GameManagerOptions localMatchConfig,
                                          const std::vector<EngineConfiguration> &configs,
                                          const std::string &fen)
{
    // Initialize variables
    std::vector<MatchInfo> matches;

    const int games = localMatchConfig.games;

    int local_wins = 0;
    int local_losses = 0;
    int local_draws = 0;

    // Game stats from white pov for white advantage
    int white_local_wins = 0;
    int white_local_losses = 0;

    // We need to keep a copy so we don't use the wrong value
    int round_copy = ++round_count_;

    for (int i = 0; i < games; i++)
    {
        auto conf1 = i == 0 ? configs[0] : configs[1];
        auto conf2 = i != 0 ? configs[0] : configs[1];

        MatchInfo match_result;
        Match m = Match(match_config_, conf1, conf2, save_time_header_);

        match_result = m.startMatch(round_copy, fen);

        if (match_result.needs_restart)
        {
            i--;
            continue;
        }

        total_count_++;
        matches.emplace_back(match_result);

        const std::string positive_engine = i == 0 ? configs[0].name : configs[1].name;
        const std::string negative_engine = i == 1 ? configs[0].name : configs[1].name;

        if (match_result.result == GameResult::WHITE_WIN)
        {
            local_wins += match_result.white_engine.name == configs[0].name ? 1 : 0;
            local_losses += match_result.white_engine.name == configs[0].name ? 0 : 1;
            white_local_wins += 1;
        }
        else if (match_result.result == GameResult::BLACK_WIN)
        {
            local_wins += match_result.black_engine.name == configs[0].name ? 1 : 0;
            local_losses += match_result.black_engine.name == configs[0].name ? 0 : 1;
            white_local_losses += 1;
        }
        else if (match_result.result == GameResult::DRAW)
        {
            local_draws++;
        }
        else
        {
            std::cout << "Couldn't obtain Game Result\n";
        }

        if (match_result.termination == "timeout")
        {
            timeouts_++;
        }

        std::stringstream ss;
        ss << "Finished game " << i + 1 << "/" << games << " in round " << round_copy << "/"
           << localMatchConfig.rounds << " total played " << total_count_ << "/"
           << localMatchConfig.rounds * games << " " << positive_engine << " vs " << negative_engine
           << ": " << resultToString(match_result.result) << "\n";

        std::cout << ss.str();
    }

    penta_WW_ += local_wins == 2 ? 1 : 0;
    penta_WD_ += local_wins == 1 && local_draws == 1 ? 1 : 0;
    penta_WL_ += (local_wins == 1 && local_losses == 1) || local_draws == 2 ? 1 : 0;
    penta_LD_ += local_losses == 1 && local_draws == 1 ? 1 : 0;
    penta_LL_ += local_losses == 2 ? 1 : 0;

    wins_ += local_wins;
    losses_ += local_losses;
    draws_ += local_draws;
    // Update white advantage stats
    white_wins_ += white_local_wins;
    white_losses_ += white_local_losses;

    if (round_count_ % localMatchConfig.ratinginterval == 0)
        printElo();

    // Write matches to file
    for (const auto &played_matches : matches)
    {
        PgnBuilder pgn(played_matches, match_config_, save_time_header_);

        writeToFile(pgn.getPGN());
    }

    return matches;
}

void Tournament::startTournament(const std::vector<EngineConfiguration> &configs)
{
    if (configs.size() < 2)
    {
        throw std::runtime_error("Warning: Need at least two engines to start!");
    }

    Logger::coutInfo("Starting tournament...");

    std::vector<std::future<std::vector<MatchInfo>>> results;

    for (int i = 1; i <= match_config_.rounds; i++)
    {
        results.emplace_back(pool_.enqueue(
            std::bind(&Tournament::runH2H, this, match_config_, configs, fetchNextFen())));
    }

    engine_names_.push_back(configs[0].name);
    engine_names_.push_back(configs[1].name);

    while (sprt_.isValid() && !pool_.stop_)
    {
        const double llr = sprt_.getLLR(wins_, draws_, losses_);
        if (sprt_.getResult(llr) != SPRT_CONTINUE)
        {
            pool_.kill();
            std::cout << "Finished match\n";
            printElo();

            return;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }

    for (auto &&result : results)
    {
        const auto res = result.get();

        if (store_pgns_)
        {
            for (const MatchInfo &match : res)
            {
                PgnBuilder pgn(match, match_config_, save_time_header_);
                pgns_.emplace_back(pgn.getPGN());
            }
        }
    }

    std::cout << "Finished match\n";
    printElo();
}

void Tournament::stopPool()
{
    pool_.kill();
}

Stats Tournament::getStats() const
{
    return Stats(wins_, draws_, losses_, penta_WW_, penta_WD_, penta_WL_, penta_LD_, penta_LL_,
                 round_count_, total_count_, timeouts_);
}

void Tournament::setStats(const Stats &stats)
{
    wins_ = stats.wins;
    losses_ = stats.losses;
    draws_ = stats.draws;

    penta_WW_ = stats.penta_WW;
    penta_WD_ = stats.penta_WD;
    penta_WL_ = stats.penta_WL;
    penta_LD_ = stats.penta_LD;
    penta_LL_ = stats.penta_LL;

    round_count_ = stats.round_count;
    total_count_ = stats.total_count;
    timeouts_ = stats.timeouts;
}

} // namespace fast_chess
