#pragma once

#include <chess.hpp>

#include <cli/cli.hpp>
#include <matchmaking/player.hpp>
#include <pgn/pgn_reader.hpp>
#include <types/match_data.hpp>

namespace fast_chess {

class DrawTracker {
   public:
    DrawTracker(const options::Tournament& tournament_config) noexcept {
        move_number_ = tournament_config.draw.move_number;
        move_count_  = tournament_config.draw.move_count;
        draw_score   = tournament_config.draw.score;
    }

    void update(const int score, const int move_count, engine::ScoreType score_type, 
                const int hmvc) noexcept {
        if (hmvc == 0) draw_moves = 0;
        if (move_count >= move_number_ && std::abs(score) <= draw_score &&
            score_type == engine::ScoreType::CP) {
            draw_moves++;
        } else {
            draw_moves = 0;
        }
    }

    [[nodiscard]] bool adjudicatable() const noexcept { return draw_moves >= move_count_ * 2; }

   private:
    // number of moves below the draw threshold
    int draw_moves = 0;
    // the score must be below this threshold to draw
    int draw_score = 0;

    // config
    int move_number_ = 0;
    int move_count_  = 0;
};

class ResignTracker {
   public:
    ResignTracker(const options::Tournament& tournament_config) noexcept {
        resign_score = tournament_config.resign.score;
        move_count_  = tournament_config.resign.move_count;
        twosided_    = tournament_config.resign.twosided;
    }

    void update(const int score, engine::ScoreType score_type, chess::Color color) noexcept {
        if ((std::abs(score) >= resign_score && score_type == engine::ScoreType::CP) ||
            score_type == engine::ScoreType::MATE) {
            if (twosided_ && (color == chess::Color::WHITE || resign_moves > 0)) resign_moves++;
        } else {
            resign_moves = 0;
        }
        if (color == chess::Color::BLACK && !twosided_) {
           if ((score <= -resign_score && score_type == engine::ScoreType::CP) || 
               (score < 0 && score_type == engine::ScoreType::MATE)) {
                resign_moves_black++;
           } else {resign_moves_black = 0;}
        } else if (!twosided_) {
           if ((score <= -resign_score && score_type == engine::ScoreType::CP) || 
               (score < 0 && score_type == engine::ScoreType::MATE)) {
                resign_moves_white++;
           } else {resign_moves_white = 0;}
       }
    }

    [[nodiscard]] bool resignable() const noexcept { 
       if (twosided_) return resign_moves >= move_count_ * 2;
       else return resign_moves_black >= move_count_ || resign_moves_white >= move_count_;
    }

   private:
    // number of moves above the resign threshold
    int resign_moves = 0;
    int resign_moves_black = 0;
    int resign_moves_white = 0;

    // config
    // the score muust be above this threshold to resign
    int resign_score = 0;
    int move_count_  = 0;
    bool twosided_   = false;
};

class MaxMovesTracker {
   public:
    MaxMovesTracker(const options::Tournament& tournament_config) noexcept {
        move_count_ = tournament_config.maxmoves.move_count;
    }

    void update() noexcept { max_moves++; }

    [[nodiscard]] bool maxmovesreached() const noexcept { return max_moves >= move_count_ * 2; }

   private:
    int max_moves   = 0;
    int move_count_ = 0;
};

class Match {
   public:
    Match(const options::Tournament& tournament_config, const pgn::Opening& opening)
        : tournament_options_(tournament_config), opening_(opening) {}

    // starts the match
    void start(engine::UciEngine& engine1, engine::UciEngine& engine2,
               const std::vector<int>& cpus);

    // returns the match data, only valid after the match has finished
    [[nodiscard]] const MatchData& get() const { return data_; }

   private:
    static bool isUciMove(const std::string& move) noexcept;
    void verifyPvLines(const Player& us);

    // Add opening moves to played moves
    void prepare();

    static void setDraw(Player& us, Player& them) noexcept;
    static void setWin(Player& us, Player& them) noexcept;
    static void setLose(Player& us, Player& them) noexcept;

    // append the move data to the match data
    void addMoveData(const Player& player, int64_t measured_time_ms, bool legal);

    // returns false if the next move could not be played
    [[nodiscard]] bool playMove(Player& us, Player& opponent);

    // returns true if adjudicated
    [[nodiscard]] bool adjudicate(Player& us, Player& them) noexcept;

    [[nodiscard]] static std::string convertChessReason(const std::string& engine_name,
                                                        chess::GameResultReason reason) noexcept;

    bool isLegal(chess::Move move) const noexcept;

    const options::Tournament& tournament_options_;
    const pgn::Opening& opening_;

    MatchData data_     = {};
    chess::Board board_ = chess::Board();

    DrawTracker draw_tracker_         = DrawTracker(tournament_options_);
    ResignTracker resign_tracker_     = ResignTracker(tournament_options_);
    MaxMovesTracker maxmoves_tracker_ = MaxMovesTracker(tournament_options_);

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
