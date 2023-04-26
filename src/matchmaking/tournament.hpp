#pragma once

#include "../options.hpp"
#include "../sprt.hpp"
#include "file_writer.hpp"
#include "result.hpp"
#include "threadpool.hpp"

namespace fast_chess {
class Tournament {
   public:
    explicit Tournament(const CMD::GameManagerOptions &game_config);

    void loadConfig(const CMD::GameManagerOptions &game_config);

    void start(const std::vector<EngineConfiguration> &engine_configs);
    void stop();

   private:
    void validateConfig(const std::vector<EngineConfiguration> &configs);

    /// @brief fetches the next fen from a sequential read opening book or from a randomized opening
    /// book order
    /// @return
    std::string fetchNextFen();

    CMD::GameManagerOptions game_config_ = {};

    ThreadPool pool_ = ThreadPool(1);

    SPRT sprt_ = SPRT();

    Result result_ = Result();

    FileWriter file_writer_;

    /// @brief contains all opening fens
    std::vector<std::string> opening_book_;
};

}  // namespace fast_chess