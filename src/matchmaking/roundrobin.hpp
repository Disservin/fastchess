#pragma once

#include "../options.hpp"
#include "../sprt.hpp"
#include "file_writer.hpp"
#include "match.hpp"
#include "output/output_fastchess.hpp"
#include "result.hpp"
#include "threadpool.hpp"
#include "types/stats.hpp"

namespace fast_chess {
class RoundRobin {
   public:
    RoundRobin(const CMD::GameManagerOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);

   private:
    void create(const std::vector<EngineConfiguration> &engine_configs,
                std::vector<std::future<void>> &results);

    void createPairings(const EngineConfiguration &player1, const EngineConfiguration &player2);

    std::pair<bool, Stats> playGame(
        const std::pair<EngineConfiguration, EngineConfiguration> &configs, const std::string &fen,
        int round_id);

    Stats updateStats(const MatchData &match_data);

    /// @brief fetches the next fen from a sequential read opening book or from a randomized
    /// opening book order
    /// @return
    [[nodiscard]] std::string fetchNextFen();

    std::unique_ptr<Output> output_;

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    Result result_ = Result();

    FileWriter file_writer_;

    /// @brief contains all opening fens
    std::vector<std::string> opening_book_;

    std::atomic<uint64_t> total = 0;
};
}  // namespace fast_chess
