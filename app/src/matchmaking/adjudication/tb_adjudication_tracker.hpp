#pragma once

#include <chess.hpp>
#include <core/config/config.hpp>
#include <matchmaking/syzygy.hpp>

namespace fastchess {

class TbAdjudicationTracker {
   public:
    TbAdjudicationTracker(const int max_pieces, const bool ignore_50_move_rule)
        : max_pieces_(max_pieces), ignore_50_move_rule_(ignore_50_move_rule) {}

    TbAdjudicationTracker(config::TbAdjudication tb_adjudication)
        : TbAdjudicationTracker(tb_adjudication.max_pieces, tb_adjudication.ignore_50_move_rule) {}

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

}  // namespace fastchess
