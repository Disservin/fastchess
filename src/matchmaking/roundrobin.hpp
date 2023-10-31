#pragma once

#include <matchmaking/match.hpp>
#include <matchmaking/result.hpp>
#include <util/file_writer.hpp>
#include <util/threadpool.hpp>
#include <pgn_reader.hpp>
#include <sprt.hpp>
#include <types/stats.hpp>
#include <affinity/cores.hpp>
#include <util/rand.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class RoundRobin {
   public:
    explicit RoundRobin(const cmd::TournamentOptions &game_config);

    /// @brief starts the round robin
    /// @param engine_configs
    void start(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief forces the round robin to stop
    void stop() {
        atomic::stop = true;
        Logger::cout("Stopped round robin!");
        pool_.kill();
    }

    [[nodiscard]] stats_map getResults() noexcept { return result_.getResults(); }
    void setResults(const stats_map &results) noexcept { result_.setResults(results); }

    void setGameConfig(const cmd::TournamentOptions &game_config) noexcept {
        tournament_options_ = game_config;
    }

   private:
    /// @brief load an pgn opening book
    void setupPgnOpeningBook();
    /// @brief load an epd opening book
    void setupEpdOpeningBook();

    /// @brief creates the matches
    /// @param engine_configs
    /// @param results
    void create(const std::vector<EngineConfiguration> &engine_configs);

    /// @brief update the current running sprt. SPRT Config has to be valid.
    /// @param engine_configs
    void updateSprtStatus(const std::vector<EngineConfiguration> &engine_configs);

    using start_callback    = std::function<void()>;
    using finished_callback = std::function<void(const Stats &stats, const std::string &reason)>;

    /// @brief play one game and write it to the pgn file
    /// @param configs
    /// @param opening
    /// @param round_id
    /// @param start
    /// @param finish
    void playGame(const std::pair<EngineConfiguration, EngineConfiguration> &configs,
                  start_callback start, finished_callback finish, const Opening &opening,
                  std::size_t round_id);

    /// @brief create the Stats object from the match data
    /// @param match_data
    /// @return
    [[nodiscard]] static Stats extractStats(const MatchData &match_data);

    /// @brief fetches the next fen from a sequential read opening book or from a randomized
    /// opening book order
    /// @return
    [[nodiscard]] Opening fetchNextOpening();

    /// @brief Fisher-Yates / Knuth shuffle
    /// @tparam T
    /// @param vec
    template <typename T>
    void shuffle(std::vector<T> &vec) {
        if (tournament_options_.opening.order == OrderType::RANDOM) {
            for (std::size_t i = 0; i + 2 <= vec.size(); i++) {
                std::size_t j = i + (random::mersenne_rand() % (vec.size() - i));
                std::swap(vec[i], vec[j]);
            }
        }
    }

    /// @brief Outputs the current state of the round robin to the console
    std::unique_ptr<IOutput> output_;

    cmd::TournamentOptions tournament_options_ = {};

    /// @brief the file writer for the pgn file
    FileWriter file_writer_;

    ThreadPool pool_ = ThreadPool(1);

    Result result_ = Result();

    SPRT sprt_ = SPRT();

    affinity::CoreHandler cores_;

    std::vector<std::string> opening_book_epd_;
    std::vector<Opening> opening_book_pgn_;

    /// @brief number of games played
    std::atomic<uint64_t> match_count_ = 0;
    /// @brief number of games to be played
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fast_chess
