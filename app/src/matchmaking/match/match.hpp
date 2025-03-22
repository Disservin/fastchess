#pragma once

#include <chess.hpp>

#include <cli/cli.hpp>
#include <core/config/config.hpp>
#include <game/book/opening_book.hpp>
#include <matchmaking/player.hpp>
#include <matchmaking/syzygy.hpp>
#include <types/match_data.hpp>

namespace fastchess {

class DrawTracker {
   public:
    DrawTracker(uint32_t move_number, int move_count, int draw_score)
        : draw_score_(draw_score), move_number_(move_number), move_count_(move_count) {}

    void update(const int score, engine::ScoreType score_type, const int hmvc) noexcept {
        if (hmvc == 0) draw_moves_ = 0;

        if (move_count_ > 0) {
            if (std::abs(score) <= draw_score_ && score_type == engine::ScoreType::CP) {
                draw_moves_++;
            } else {
                draw_moves_ = 0;
            }
        }
    }

    [[nodiscard]] bool adjudicatable(uint32_t plies) const noexcept {
        return plies >= move_number_ && draw_moves_ >= move_count_ * 2;
    }

   private:
    // number of moves below the draw threshold
    int draw_moves_ = 0;
    // the score must be below this threshold to draw
    int draw_score_;

    // config
    uint32_t move_number_;
    int move_count_;
};

class ResignTracker {
   public:
    ResignTracker(int resign_score, int move_count, bool twosided) noexcept
        : resign_score(resign_score), move_count_(move_count), twosided_(twosided) {}

    void update(const int score, engine::ScoreType score_type, chess::Color color) noexcept {
        if (twosided_) {
            if ((std::abs(score) >= resign_score && score_type == engine::ScoreType::CP) ||
                score_type == engine::ScoreType::MATE) {
                resign_moves++;
            } else {
                resign_moves = 0;
            }
        } else {
            int& counter = (color == chess::Color::BLACK) ? resign_moves_black : resign_moves_white;
            if ((score <= -resign_score && score_type == engine::ScoreType::CP) ||
                (score < 0 && score_type == engine::ScoreType::MATE)) {
                counter++;
            } else {
                counter = 0;
            }
        }
    }

    [[nodiscard]] bool resignable() const noexcept {
        if (twosided_) return resign_moves >= move_count_ * 2;
        return resign_moves_black >= move_count_ || resign_moves_white >= move_count_;
    }

   private:
    // number of moves above the resign threshold
    int resign_moves       = 0;
    int resign_moves_black = 0;
    int resign_moves_white = 0;

    // config
    // the score muust be above this threshold to resign
    int resign_score;
    int move_count_;
    bool twosided_;
};

class MaxMovesTracker {
   public:
    MaxMovesTracker(int move_count) noexcept : move_count_(move_count) {}

    void update() noexcept { max_moves++; }

    [[nodiscard]] bool maxmovesreached() const noexcept { return max_moves >= move_count_ * 2; }

   private:
    int max_moves = 0;
    int move_count_;
};

class TbAdjudicationTracker {
   public:
    TbAdjudicationTracker(const int max_pieces, const bool ignore_50_move_rule)
        : max_pieces_(max_pieces), ignore_50_move_rule_(ignore_50_move_rule) {}

    [[nodiscard]] bool adjudicatable(const chess::Board& board) const noexcept {
        if (max_pieces_ != 0 && board.occ().count() > max_pieces_) {
            return false;
        }
        return canProbeSyzgyWdl(board);
    }

    [[nodiscard]] chess::GameResult adjudicate(const chess::Board& board) const noexcept {
        return probeSyzygyWdl(board, ignore_50_move_rule_);
    }

   private:
    int max_pieces_;
    bool ignore_50_move_rule_;
};

class Match {
   public:
    Match(const book::Opening& opening);

    // starts the match
    void start(engine::UciEngine& white, engine::UciEngine& black, const std::vector<int>& cpus);

    // returns the match data, only valid after the match has finished
    [[nodiscard]] const MatchData& get() const { return data_; }

    [[nodiscard]] bool isStallOrDisconnect() const noexcept { return stall_or_disconnect_; }

    static std::string convertScoreToString(int score, engine::ScoreType score_type);

   private:
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
    [[nodiscard]] bool validConnection(Player& us, Player& them);

    // returns true if adjudicated
    [[nodiscard]] bool adjudicate(Player& us, Player& them) noexcept;

    [[nodiscard]] static std::string convertChessReason(const std::string&, chess::GameResultReason) noexcept;

    bool isLegal(chess::Move move) const noexcept;

    const book::Opening& opening_;

    MatchData data_     = {};
    chess::Board board_ = chess::Board();

    DrawTracker draw_tracker_;
    ResignTracker resign_tracker_;
    MaxMovesTracker maxmoves_tracker_;
    TbAdjudicationTracker tb_adjudication_tracker_;

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
