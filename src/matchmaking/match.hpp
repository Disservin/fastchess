#pragma once

#include <third_party/chess.hpp>

#include <cli.hpp>
#include <matchmaking/participant.hpp>
#include <matchmaking/types/match_data.hpp>
#include <pgn_reader.hpp>

namespace fast_chess {

struct DrawTacker {
    /// @brief number of moves below the draw threshold
    std::size_t draw_moves = 0;
    /// @brief the score must be below this threshold to draw
    int draw_score = 0;
};

struct ResignTracker {
    /// @brief number of moves above the resign threshold
    std::size_t resign_moves = 0;
    /// @brief the score muust be above this threshold to resign
    int resign_score = 0;
};

class Match {
   public:
    Match(const cmd::TournamentOptions& game_config, const Opening& opening, int round) {
        tournament_options_ = game_config;
        data_.round         = round;

        opening_ = opening;
    }

    /// @brief starts the match
    void start(const EngineConfiguration& engine1_config,
               const EngineConfiguration& engine2_config);

    /// @brief returns the match data, only valid after the match has finished
    [[nodiscard]] MatchData get() const { return data_; }

   private:
    void verifyPv(const Participant& us);

    void setDraw(Participant& us, Participant& them);
    void setWin(Participant& us, Participant& them);
    void setLose(Participant& us, Participant& them);

    /// @brief append the move data to the match data
    /// @param player
    /// @param measured_time
    void addMoveData(Participant& player, int64_t measured_time);

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

    [[nodiscard]] static std::string convertChessReason(const std::string& engine_name,
                                                        chess::GameResultReason reason);

    inline static const std::string ADJUDICATION_MSG      = "Draw by adjudication";
    inline static const std::string ADJUDICATION_WIN_MSG  = " wins by adjudication";
    inline static const std::string ADJUDICATION_LOSE_MSG = " loses by adjudication";

    inline static const std::string INSUFFICIENT_MSG = "Draw by insufficient material";
    inline static const std::string REPETITION_MSG   = "Draw by 3-fold repetition";
    inline static const std::string FIFTY_MSG        = "Draw by 50-move rule";
    inline static const std::string DISCONNECT_MSG   = /*.. */ " disconnects";
    inline static const std::string TIMEOUT_MSG      = /*.. */ " loses on time";
    inline static const std::string CHECKMATE_MSG    = /*..*/ " got checkmated";
    inline static const std::string STALEMATE_MSG    = "Draw by stalemate";
    inline static const std::string ILLEGAL_MSG      = " made an illegal move";

    DrawTacker draw_tracker_;
    ResignTracker resign_tracker_;

    cmd::TournamentOptions tournament_options_;
    chess::Board board_;
    MatchData data_;

    // keeps track of the moves played in the match, required for the
    // uci position command
    std::vector<std::string> played_moves_;

    Opening opening_;

    // start position, required for the uci position command
    // is either startpos or the fen of the opening
    std::string start_position;
};
}  // namespace fast_chess
