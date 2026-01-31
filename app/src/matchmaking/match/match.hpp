#pragma once

#include <chess.hpp>

#include <cli/cli.hpp>
#include <core/config/config.hpp>
#include <game/book/opening_book.hpp>
#include <matchmaking/player.hpp>
#include <matchmaking/syzygy.hpp>
#include <types/match_data.hpp>

#include <matchmaking/adjudication/adjudicator.hpp>

namespace fastchess {

class Match {
   public:
    Match(const book::Opening& opening);

    // starts the match
    void start(engine::UciEngine& white, engine::UciEngine& black, std::optional<std::vector<int>>& cpus);

    // returns the match data, only valid after the match has finished
    [[nodiscard]] const MatchData& get() const { return data_; }

    [[nodiscard]] bool isStallOrDisconnect() const noexcept { return stall_or_disconnect_; }

    static std::string convertScoreToString(int score, engine::ScoreType score_type);

   private:
    void gameLoop(Player& first, Player& second);

    // returns the reason and the result of the game, different order than chess lib function
    [[nodiscard]] std::pair<chess::GameResultReason, chess::GameResult> isGameOver() const;

    void setEngineCrashStatus(Player& loser, Player& winner);
    void setEngineStallStatus(Player& loser, Player& winner);
    void setEngineTimeoutStatus(Player& loser, Player& winner);
    void setEngineIllegalMoveStatus(Player& loser, Player& winner, const std::optional<std::string>& best_move,
                                    bool invalid_format = false);

    void verifyPvLines(const Player& us);

    // append the move data to the match data
    void addMoveData(const Player& player, int64_t latency, int64_t measured_time_ms, int64_t timeleft, bool legal);

    // returns false if the next move could not be played
    [[nodiscard]] bool playMove(Player& us, Player& them);

    // returns false if the connection is invalid
    bool validConnection(Player& us, Player& them);

    // returns true if adjudicated
    [[nodiscard]] bool adjudicate(Player& us, Player& them) noexcept;

    [[nodiscard]] static std::string convertChessReason(const std::string&, chess::GameResultReason) noexcept;

    bool isLegal(chess::Move move) const noexcept;

    const book::Opening& opening_;

    MatchData data_     = {};
    chess::Board board_ = chess::Board();

    Adjudicator adjudicator_;

    std::vector<std::string> uci_moves_;

    // start position, required for the uci position command
    // is either startpos or the fen of the opening
    std::string start_position_;

    bool stall_or_disconnect_ = false;

    inline static constexpr char INSUFFICIENT_MSG[]         = "Draw by insufficient mating material";
    inline static constexpr char REPETITION_MSG[]           = "Draw by 3-fold repetition";
    inline static constexpr char ILLEGAL_MSG[]              = " makes an illegal move";
    inline static constexpr char ADJUDICATION_WIN_MSG[]     = " wins by adjudication";
    inline static constexpr char ADJUDICATION_TB_WIN_MSG[]  = " wins by adjudication: SyzygyTB";
    inline static constexpr char ADJUDICATION_MSG[]         = "Draw by adjudication";
    inline static constexpr char ADJUDICATION_TB_DRAW_MSG[] = "Draw by adjudication: SyzygyTB";
    inline static constexpr char FIFTY_MSG[]                = "Draw by fifty moves rule";
    inline static constexpr char STALEMATE_MSG[]            = "Draw by stalemate";
    inline static constexpr char CHECKMATE_MSG[]            = /*..*/ " mates";
    inline static constexpr char TIMEOUT_MSG[]              = /*.. */ " loses on time";
    inline static constexpr char DISCONNECT_MSG[]           = /*.. */ " disconnects";
    inline static constexpr char STALL_MSG[]                = /*.. */ "'s connection stalls";
    inline static constexpr char INTERRUPTED_MSG[]          = "Game interrupted";
};
}  // namespace fastchess
