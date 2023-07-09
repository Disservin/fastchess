#pragma once

#include <chess.hpp>

#include <cli.hpp>
#include <matchmaking/participant.hpp>
#include <pgn_reader.hpp>
#include <types/match_data.hpp>

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
    Match(const cmd::TournamentOptions& game_config, const Opening& opening,
          const std::vector<int>& cpus)
        : tournament_options_(game_config), opening_(opening), cpus_(cpus) {}

    /// @brief starts the match
    void start(UciEngine& engine1, UciEngine& engine2);

    /// @brief returns the match data, only valid after the match has finished
    [[nodiscard]] const MatchData& get() const { return data_; }

   private:
    void verifyPvLines(const Participant& us);

    /// @brief Add opening moves to played moves
    void prepareOpening();

    static void setDraw(Participant& us, Participant& them) noexcept;
    static void setWin(Participant& us, Participant& them) noexcept;
    static void setLose(Participant& us, Participant& them) noexcept;

    /// @brief append the move data to the match data
    /// @param player
    /// @param measured_time_ms
    void addMoveData(Participant& player, int64_t measured_time_ms);

    /// @brief returns false if the next move could not be played
    /// @param us
    /// @param opponent
    /// @return
    [[nodiscard]] bool playMove(Participant& us, Participant& opponent);

    void updateDrawTracker(const Participant& player) noexcept;
    void updateResignTracker(const Participant& player) noexcept;

    /// @brief returns true if adjudicated
    /// @param us
    /// @param them
    /// @return
    [[nodiscard]] bool adjudicate(Participant& us, Participant& them) noexcept;

    [[nodiscard]] static std::string convertChessReason(const std::string& engine_name,
                                                        chess::GameResultReason reason) noexcept;

    bool isLegal(chess::Move move) const noexcept;

    const cmd::TournamentOptions& tournament_options_;
    const Opening& opening_;
    const std::vector<int>& cpus_;

    MatchData data_     = {};
    chess::Board board_ = chess::Board();

    DrawTacker draw_tracker_      = {};
    ResignTracker resign_tracker_ = {};

    // start position, required for the uci position command
    // is either startpos or the fen of the opening
    std::string start_position_;

    inline static constexpr char INSUFFICIENT_MSG[]      = "Draw by insufficient material";
    inline static constexpr char REPETITION_MSG[]        = "Draw by 3-fold repetition";
    inline static constexpr char ADJUDICATION_LOSE_MSG[] = " loses by adjudication";
    inline static constexpr char ILLEGAL_MSG[]           = " made an illegal move";
    inline static constexpr char ADJUDICATION_WIN_MSG[]  = " wins by adjudication";
    inline static constexpr char ADJUDICATION_MSG[]      = "Draw by adjudication";
    inline static constexpr char FIFTY_MSG[]             = "Draw by 50-move rule";
    inline static constexpr char STALEMATE_MSG[]         = "Draw by stalemate";
    inline static constexpr char CHECKMATE_MSG[]         = /*..*/ " got checkmated";
    inline static constexpr char TIMEOUT_MSG[]           = /*.. */ " loses on time";
    inline static constexpr char DISCONNECT_MSG[]        = /*.. */ " disconnects";
};
}  // namespace fast_chess
