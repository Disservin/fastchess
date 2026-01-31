#pragma once

#include <cmath>

#include <chess.hpp>
#include <core/config/config.hpp>
#include <engine/uci_engine.hpp>

namespace fastchess {

class ResignTracker {
   public:
    ResignTracker(int resign_score, int move_count, bool twosided) noexcept
        : resign_score(resign_score), move_count_(move_count), twosided_(twosided) {}

    ResignTracker(config::ResignAdjudication resign_adjudication)
        : ResignTracker(resign_adjudication.score, resign_adjudication.move_count, resign_adjudication.twosided) {}

    void update(const engine::Score& score, chess::Color color) noexcept {
        if (twosided_) {
            if ((std::abs(score.value) >= resign_score && score.type == engine::ScoreType::CP) ||
                score.type == engine::ScoreType::MATE) {
                resign_moves++;
            } else {
                resign_moves = 0;
            }
        } else {
            int& counter = (color == chess::Color::BLACK) ? resign_moves_black : resign_moves_white;
            if ((score.value <= -resign_score && score.type == engine::ScoreType::CP) ||
                (score.value < 0 && score.type == engine::ScoreType::MATE)) {
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

    void invalidate(chess::Color color) noexcept {
        if (twosided_) {
            resign_moves = 0;
            return;
        }

        if (color == chess::Color::BLACK) {
            resign_moves_black = 0;
        } else {
            resign_moves_white = 0;
        }
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

}  // namespace fastchess
