#pragma once

#include "../options.hpp"
#include "../third_party/chess.hpp"
#include "match_data.hpp"
#include "participant.hpp"
namespace fast_chess {

struct DrawTacker {
    size_t draw_moves = 0;
    int draw_score = 0;
};

struct ResignTracker {
    size_t resign_moves = 0;
    int resign_score = 0;
};

class Match {
   public:
    Match(const CMD::GameManagerOptions& game_config, const EngineConfiguration& engine1_config,
          const EngineConfiguration& engine2_config, const std::string& fen, int round);

    MatchData get() const;

   private:
    void setDraw(Participant& us, Participant& them, const std::string& msg);
    void setWin(Participant& us, Participant& them, const std::string& msg);
    void setLose(Participant& us, Participant& them, const std::string& msg);

    void addMoveData(Participant& player, int64_t measured_time);

    void start(Participant& engine1, Participant& engine2, const std::string& fen);

    /// @brief returns false if the next move could not be played
    /// @param us
    /// @param opponent
    /// @return
    bool playMove(Participant& us, Participant& opponent);

    void updateDrawTracker(const Participant& player);
    void updateResignTracker(const Participant& player);

    /// @brief returns true if adjudicated
    /// @param us
    /// @param them
    /// @return
    bool adjudicate(Participant& us, Participant& them);

    DrawTacker draw_tracker_;
    ResignTracker resign_tracker_;

    CMD::GameManagerOptions game_config_;
    Chess::Board board_;
    MatchData data_;

    std::vector<std::string> played_moves_;

    std::string start_fen_;
};
}  // namespace fast_chess
