#pragma once

#include <vector>

#include <affinity/affinity_manager.hpp>
#include <book/opening_book.hpp>
#include <engine/uci_engine.hpp>
#include <globals/globals.hpp>
#include <matchmaking/output/output.hpp>
#include <matchmaking/scoreboard.hpp>
#include <matchmaking/tournament/generator.hpp>
#include <types/tournament.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/game_pair.hpp>
#include <util/logger/logger.hpp>
#include <util/threadpool.hpp>

namespace fastchess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class BaseTournament {
   public:
    BaseTournament(const stats_map &results);

    virtual ~BaseTournament() {
        Logger::trace("~BaseTournament()");
        saveJson();
        Logger::trace("Instructing engines to stop...");
        writeToOpenPipes();
    }

    virtual void start();
    virtual void startNext() = 0;

    [[nodiscard]] stats_map getResults() noexcept { return scoreboard_.getResults(); }

    void setResults(const stats_map &results) noexcept {
        Logger::trace("Setting results...");

        scoreboard_.setResults(results);

        match_count_ = 0;

        for (const auto &pair1 : scoreboard_.getResults()) {
            const auto &stats = pair1.second;

            match_count_ += stats.wins + stats.losses + stats.draws;
        }

        initial_matchcount_ = match_count_;
    }

   protected:
    // number of games played
    std::atomic<std::uint64_t> match_count_;
    std::uint64_t initial_matchcount_;

    // creates the matches
    virtual void create() = 0;

    using start_callback    = std::function<void()>;
    using finished_callback = std::function<void(const Stats &stats, const std::string &reason, const engines &)>;

    // Function to save the config file
    void saveJson();

    // play one game and write it to the pgn file
    void playGame(const GamePair<EngineConfiguration, EngineConfiguration> &configs, start_callback start,
                  finished_callback finish, const pgn::Opening &opening, std::size_t round_id, std::size_t game_id);

    MatchGenerator generator_;

    std::unique_ptr<IOutput> output_;
    std::unique_ptr<affinity::AffinityManager> cores_;
    std::unique_ptr<util::FileWriter> file_writer_pgn;
    std::unique_ptr<util::FileWriter> file_writer_epd;
    std::unique_ptr<book::OpeningBook> book_;

    util::CachePool<engine::UciEngine, std::string> engine_cache_ = util::CachePool<engine::UciEngine, std::string>();
    ScoreBoard scoreboard_                                        = ScoreBoard();
    util::ThreadPool pool_                                        = util::ThreadPool(1);

   private:
    int getMaxAffinity(const std::vector<EngineConfiguration> &configs) const noexcept;
};

}  // namespace fastchess
