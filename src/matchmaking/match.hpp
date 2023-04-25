#pragma once

#include "../options.hpp"
#include "../third_party/chess.hpp"
#include "match_data.hpp"
#include "participant.hpp"

namespace fast_chess {
class Match {
   public:
    Match(const CMD::GameManagerOptions& game_config, const EngineConfiguration& engine1_config,
          const EngineConfiguration& engine2_config, const std::string& fen, int round);

    MatchData get() const;

   private:
    void start(const std::string& fen);
    bool playMove();

    std::vector<std::string> played_moves_;

    UciEngine engine1_;
    UciEngine engine2_;

    MatchData data_;
};
}  // namespace fast_chess
