#pragma once

#include <affinity/affinity_manager.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/scoreboard.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <matchmaking/stats.hpp>
#include <pgn/pgn_reader.hpp>
#include <types/tournament.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/rand.hpp>
#include <util/threadpool.hpp>

#include <matchmaking/tournament/base/tournament.hpp>

namespace fastchess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class RoundRobin : public BaseTournament {
   public:
    explicit RoundRobin(const stats_map& results);

    ~RoundRobin() { Logger::trace("~RoundRobin()"); }

    // starts the round robin
    void start() override;

    void startNext() override;

   protected:
    // creates the matches
    void create() override;

    void createMatch(std::size_t i, std::size_t j, std::size_t round_id, int g, std::optional<std::size_t> opening_id);

   private:
    void createMatch(std::size_t i, std::size_t j, std::size_t round_id, int g, std::optional<std::size_t> opening_id);

    // update the current running sprt. SPRT Config has to be valid.
    void updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs, const engines& engines);

    SPRT sprt_ = SPRT();

    std::mutex output_mutex_;
    std::mutex game_gen_mutex_;

    // number of games to be played
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fastchess
