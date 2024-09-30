#pragma once

#include <affinity/affinity_manager.hpp>
#include <book/opening_book.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/scoreboard.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <matchmaking/stats.hpp>
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

   protected:
    void startNext() override;

    // creates the matches
    void create() override;

   private:
    bool shouldPrintRatingInterval(std::size_t round_id) const noexcept {
        const auto& cfg = config::TournamentConfig.get();

        // round_id and match_count_ starts 0 so we add 1
        const auto ratinginterval_index = cfg.report_penta ? round_id + 1 : match_count_ + 1;

        return ratinginterval_index % cfg.ratinginterval == 0;
    }

    // print score result based on scoreinterval if output format is cutechess
    bool shouldPrintScoreInterval() const noexcept {
        const auto& cfg = config::TournamentConfig.get();

        const auto scoreinterval_index = match_count_ + 1;

        return scoreinterval_index % cfg.scoreinterval == 0;
    }

    bool allMatchesPlayed() const noexcept { return match_count_ + 1 == total_; }

    void createMatch(const MatchGenerator::Pairing& pairing);

    // update the current running sprt. SPRT Config has to be valid.
    void updateSprtStatus(const std::vector<EngineConfiguration>& engine_configs, const engines& engines);

    SPRT sprt_ = SPRT();

    std::mutex output_mutex_;
    std::mutex game_gen_mutex_;

    // number of games to be played
    std::atomic<uint64_t> total_ = 0;
};
}  // namespace fastchess
