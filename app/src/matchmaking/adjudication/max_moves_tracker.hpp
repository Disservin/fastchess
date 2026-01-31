#pragma once

#include <core/config/config.hpp>

namespace fastchess {

class MaxMovesTracker {
   public:
    MaxMovesTracker(int move_count) noexcept : move_count_(move_count) {}
    MaxMovesTracker(config::MaxMovesAdjudication maxmoves_adjudication)
        : MaxMovesTracker(maxmoves_adjudication.move_count) {}

    void update() noexcept { max_moves++; }

    [[nodiscard]] bool maxmovesreached() const noexcept { return max_moves >= move_count_ * 2; }

   private:
    int max_moves = 0;
    int move_count_;
};

}  // namespace fastchess
