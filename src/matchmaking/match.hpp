#pragma once

#include "../options.hpp"
#include "../third_party/chess.hpp"
#include "./types/match_data.hpp"
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

    [[nodiscard]] MatchData get() const;

   private:
    void verifyPv(const Participant& us);

    void setDraw(Participant& us, Participant& them, const std::string& msg,
                 const std::string& reason);
    void setWin(Participant& us, Participant& them, const std::string& msg,
                const std::string& reason);
    void setLose(Participant& us, Participant& them, const std::string& msg,
                 const std::string& reason);

    void addMoveData(Participant& player, int64_t measured_time);

    void start(Participant& engine1, Participant& engine2, const std::string& fen);

    /// @brief returns false if the next move could not be played
    /// @param us
    /// @param opponent
    /// @return
    [[nodiscard]] bool playMove(Participant& us, Participant& opponent);

    void updateDrawTracker(const Participant& player);
    void updateResignTracker(const Participant& player);

    /// @brief returns true if adjudicated
    /// @param us
    /// @param them
    /// @return
    [[nodiscard]] bool adjudicate(Participant& us, Participant& them);

    [[nodiscard]] std::string convertChessReason(const std::string& engine_name,
                                                 std::string_view reason);

    inline static const std::string ADJUDICATION_MSG = "Draw by adjudication";
    inline static const std::string ADJUDICATION_WIN_MSG = " wins by adjudication";
    inline static const std::string ADJUDICATION_LOSE_MSG = " losses by adjudication";

    inline static const std::string INSUFFICIENT_MSG = "Draw by insufficient material";
    inline static const std::string REPETITION_MSG = "Draw by 3-fold repetition";
    inline static const std::string FIFTY_MSG = "Draw by 50-move rule";
    inline static const std::string TIMEOUT_MSG = /*.. */ " timed out";
    inline static const std::string CHECKMATE_MSG = /*..*/ " got checkmated";
    inline static const std::string STALEMATE_MSG = "Draw by stalemate";
    inline static const std::string ILLEGAL_MSG = " made an illegal move";

    DrawTacker draw_tracker_;
    ResignTracker resign_tracker_;

    CMD::GameManagerOptions game_config_;
    Chess::Board board_;
    MatchData data_;

    std::vector<std::string> played_moves_;

    std::string start_fen_;
};
}  // namespace fast_chess
