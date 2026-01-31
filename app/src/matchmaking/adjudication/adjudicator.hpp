#pragma once

#include <optional>

#include <chess.hpp>
#include <core/config/config.hpp>
#include <types/match_data.hpp>  // For MatchTermination
#include <types/tournament.hpp>

#include <matchmaking/adjudication/draw_tracker.hpp>
#include <matchmaking/adjudication/max_moves_tracker.hpp>
#include <matchmaking/adjudication/resign_tracker.hpp>
#include <matchmaking/adjudication/tb_adjudication_tracker.hpp>

namespace fastchess {

class Adjudicator {
   public:
    Adjudicator(const config::Tournament& config)
        : draw_tracker_(config.draw),
          resign_tracker_(config.resign),
          maxmoves_tracker_(config.maxmoves),
          tb_adjudication_tracker_(config.tb_adjudication),
          tb_config_(config.tb_adjudication),
          resign_config_(config.resign),
          draw_config_(config.draw),
          maxmoves_config_(config.maxmoves) {}

    struct Result {
        MatchTermination termination;
        std::string reason;
        chess::GameResult result_us;    // Result for the player passed as 'us' (the one being checked)
        chess::GameResult result_them;  // Result for the other player
    };

    // Updates the trackers with the latest move/score information
    void update(const std::optional<engine::Score>& score, const chess::Board& board,
                const chess::Color side_to_move) noexcept {
        if (score.has_value()) {
            draw_tracker_.update(score.value(), board.halfMoveClock());
            resign_tracker_.update(score.value(), ~side_to_move);
        } else {
            draw_tracker_.invalidate();
            resign_tracker_.invalidate(~side_to_move);
        }

        maxmoves_tracker_.update();
    }

    // Checks if the game should be adjudicated.
    // 'us' is usually the player who is NOT to move (the one whose score we are checking),
    // but the semantics depend on the specific tracker.
    // For Resign: checks if 'us' wants to resign (based on 'score').
    // For TB: checks board state.
    // For Draw: checks move number.
    // For MaxMoves: checks total moves.
    [[nodiscard]] std::optional<Result> adjudicate(const chess::Board& board,
                                                   const std::optional<engine::Score>& score) const noexcept {
        // Start with TB adjudication
        if (tb_config_.enabled && tb_adjudication_tracker_.adjudicatable(board)) {
            const auto result       = tb_adjudication_tracker_.adjudicate(board);
            const auto desired_adju = tb_config_.result_type;

            if ((result == chess::GameResult::WIN || result == chess::GameResult::LOSE) &&
                desired_adju & config::TbAdjudication::ResultType::WIN_LOSS) {
                Result res;
                res.termination = MatchTermination::ADJUDICATION;

                // If result is WIN (for side to move), then side to move wins.
                // In Match::adjudicate(us, them), 'us' is the one NOT to move (passed as first arg).
                // Wait, let's keep it simple.
                // The TB result is relative to the side to move (board.sideToMove()).

                chess::Color winner_color = chess::Color::NONE;

                if (result == chess::GameResult::WIN) {
                    winner_color = board.sideToMove();
                } else {
                    winner_color = ~board.sideToMove();
                }

                res.reason = winner_color.longStr() + " wins by adjudication: SyzygyTB";

                // We need to know which player corresponds to which color to set GameResult correctly
                // But here we return generic GameResult.
                // Let's assume the caller maps it.
                // Actually, let's stick to what Match did.
                // Match::adjudicate(us, them). us is NOT side to move. board.sideToMove() is 'them'.
                // If board.sideToMove() wins (them wins), then 'us' loses.

                // Let's just return the GameResult relative to White/Black or SideToMove?
                // Returning a struct with specific WIN/LOSS for the passed "us" player is cleaner.

                // In Match::adjudicate(us, them):
                // us is the player we are checking (usually previous mover).
                // them is the player to move.

                // if result == WIN (for side to move, i.e. 'them'):
                // us (previous mover) loses. them wins.

                // Let's re-read Match::adjudicate carefully.
                // Match::adjudicate(Player& us, Player& them)
                // Called as adjudicate(them, us) from playMove(us, them).
                // So inside adjudicate:
                // Arg 'us' is 'them' (from playMove context) -> The player who JUST MOVED.
                // Arg 'them' is 'us' (from playMove context) -> The player TO MOVE.
                // board.sideToMove() is 'them' (from adjudicate context) -> Player TO MOVE.

                // Logic:
                // if result == WIN (means side to move wins):
                // us (arg1) setLost(). them (arg2) setWon().
                // us (arg1) is player who just moved.
                // So if side to move wins, the player who just moved loses?
                // Yes, because TB says the position is lost for the side who just moved?
                // No, TB says result for side to move.
                // If side to move is winning, then previous mover gave a winning position (blunder?).
                // Or previous mover is losing.

                // Match code:
                // if (result == GameResult::WIN) {
                //    us.setLost(); (Player who just moved LOSES)
                //    them.setWon(); (Player to move WINS)
                // }

                if (result == chess::GameResult::WIN) {
                    return Result{MatchTermination::ADJUDICATION, res.reason,
                                  chess::GameResult::LOSE,  // us
                                  chess::GameResult::WIN};  // them
                } else {
                    return Result{MatchTermination::ADJUDICATION, res.reason,
                                  chess::GameResult::WIN,    // us
                                  chess::GameResult::LOSE};  // them
                }
            }

            if (result == chess::GameResult::DRAW && desired_adju & config::TbAdjudication::ResultType::DRAW) {
                return Result{MatchTermination::ADJUDICATION, "Draw by adjudication: SyzygyTB", chess::GameResult::DRAW,
                              chess::GameResult::DRAW};
            }
        }

        // Resign Adjudication
        if (resign_config_.enabled && resign_tracker_.resignable() && score.has_value() && score.value().value < 0) {
            // Match code:
            // us.setLost();
            // them.setWon();
            // reason: color + " wins by adjudication"
            // color is board_.sideToMove().

            std::string reason = board.sideToMove().longStr() + " wins by adjudication";

            return Result{MatchTermination::ADJUDICATION, reason, chess::GameResult::LOSE, chess::GameResult::WIN};
        }

        // Draw Adjudication
        if (draw_config_.enabled && draw_tracker_.adjudicatable(board.fullMoveNumber() - 1)) {
            return Result{MatchTermination::ADJUDICATION, "Draw by adjudication", chess::GameResult::DRAW,
                          chess::GameResult::DRAW};
        }

        // Max Moves Adjudication
        if (maxmoves_config_.enabled && maxmoves_tracker_.maxmovesreached()) {
            return Result{MatchTermination::ADJUDICATION, "Draw by adjudication", chess::GameResult::DRAW,
                          chess::GameResult::DRAW};
        }

        return std::nullopt;
    }

   private:
    DrawTracker draw_tracker_;
    ResignTracker resign_tracker_;
    MaxMovesTracker maxmoves_tracker_;
    TbAdjudicationTracker tb_adjudication_tracker_;

    config::TbAdjudication tb_config_;
    config::ResignAdjudication resign_config_;
    config::DrawAdjudication draw_config_;
    config::MaxMovesAdjudication maxmoves_config_;
};

}  // namespace fastchess
