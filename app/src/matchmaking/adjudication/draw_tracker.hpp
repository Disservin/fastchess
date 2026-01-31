#pragma once

#include <cmath>
#include <cstdint>

#include <core/config/config.hpp>
#include <engine/uci_engine.hpp>

namespace fastchess {

class DrawTracker {
   public:
    DrawTracker(uint32_t move_number, int move_count, int draw_score)
        : draw_score_(draw_score), move_number_(move_number), move_count_(move_count) {}

    DrawTracker(config::DrawAdjudication draw_adjudication)
        : DrawTracker(draw_adjudication.move_number, draw_adjudication.move_count, draw_adjudication.score) {}

    void update(const engine::Score& score, const int hmvc) noexcept {
        if (hmvc == 0) draw_moves_ = 0;

        if (move_count_ > 0) {
            if (std::abs(score.value) <= draw_score_ && score.type == engine::ScoreType::CP) {
                draw_moves_++;
            } else {
                draw_moves_ = 0;
            }
        }
    }

    [[nodiscard]] bool adjudicatable(uint32_t plies) const noexcept {
        return plies >= move_number_ && draw_moves_ >= move_count_ * 2;
    }

    void invalidate() { draw_moves_ = 0; }

   private:
    // number of moves below the draw threshold
    int draw_moves_ = 0;
    // the score must be below this threshold to draw
    int draw_score_;

    // config
    uint32_t move_number_;
    int move_count_;
};

}  // namespace fastchess
