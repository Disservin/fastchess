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
    void start(Participant& engine1, Participant& engine2, const std::string& fen);
    bool playMove(Participant& us, Participant& opponent);

    CMD::GameManagerOptions game_config_;
    std::vector<std::string> played_moves_;
    MatchData data_;
    Chess::Board board_;

    std::string start_fen_;
};
}  // namespace fast_chess
