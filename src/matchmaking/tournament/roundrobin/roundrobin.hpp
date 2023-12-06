#pragma once

#include <affinity/affinity_manager.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/result.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <pgn/pgn_reader.hpp>
#include <types/stats.hpp>
#include <types/tournament_options.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/rand.hpp>
#include <util/threadpool.hpp>

#include <matchmaking/tournament/base/tournament.hpp>

namespace fast_chess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

class RoundRobin : public BaseTournament {
   public:
    explicit RoundRobin(const options::Tournament &tournament_config,
                        const std::vector<EngineConfiguration> &engine_configs);

    /// @brief starts the round robin
    void start() override;

   private:
    /// @brief creates the matches
    void create() override;

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

    SPRT sprt_ = SPRT();

    /// @brief number of games played
    std::atomic<uint64_t> match_count_ = 0;
    /// @brief number of games to be played
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fast_chess
