#pragma once

#include <core/helper.hpp>
#include <types/match_data.hpp>

namespace fastchess {

class Stats {
   public:
    Stats() = default;
    Stats(int wins, int losses, int draws) : wins(wins), losses(losses), draws(draws) {}
    Stats(int ll, int ld, int wl, int dd, int wd, int ww) {
        penta_LL = ll;
        penta_LD = ld;
        penta_WL = wl;
        penta_DD = dd;
        penta_WD = wd;
        penta_WW = ww;
    }
    Stats(const MatchData &match_data) {
        if (match_data.players.white.result == chess::GameResult::WIN) {
            wins++;
        } else if (match_data.players.white.result == chess::GameResult::LOSE) {
            losses++;
        } else if (match_data.players.white.result == chess::GameResult::DRAW ||
                   match_data.players.white.result == chess::GameResult::NONE) {
            draws++;
        }
    }

    Stats &operator+=(const Stats &rhs) {
        wins += rhs.wins;
        losses += rhs.losses;
        draws += rhs.draws;

        penta_WW += rhs.penta_WW;
        penta_WD += rhs.penta_WD;
        penta_WL += rhs.penta_WL;
        penta_DD += rhs.penta_DD;
        penta_LD += rhs.penta_LD;
        penta_LL += rhs.penta_LL;
        return *this;
    }

    [[nodiscard]] Stats operator+(const Stats &rhs) const {
        Stats stats = *this;
        stats += rhs;
        return stats;
    }

    // invert the stats
    [[nodiscard]] Stats operator~() const {
        Stats stats = *this;
        std::swap(stats.wins, stats.losses);
        std::swap(stats.penta_WW, stats.penta_LL);
        std::swap(stats.penta_WD, stats.penta_LD);
        return stats;
    }

    [[nodiscard]] bool operator==(const Stats &rhs) const {
        return wins == rhs.wins && losses == rhs.losses && draws == rhs.draws && penta_WW == rhs.penta_WW &&
               penta_WD == rhs.penta_WD && penta_WL == rhs.penta_WL && penta_DD == rhs.penta_DD &&
               penta_LD == rhs.penta_LD && penta_LL == rhs.penta_LL;
    }

    [[nodiscard]] bool operator!=(const Stats &rhs) const { return !(*this == rhs); }

    [[nodiscard]] int sum() const { return wins + losses + draws; }

    [[nodiscard]] double points() const { return wins + 0.5 * draws; }

    [[nodiscard]] double wl_dd_Ratio() const { return static_cast<double>(penta_WL) / penta_DD; }

    [[nodiscard]] double drawRatio() const { return 100.0 * draws / static_cast<double>(sum()); }

    [[nodiscard]] double drawRatioPenta() const {
        return ((penta_WL + penta_DD) / static_cast<double>(totalPairs())) * 100.0;
    }

    [[nodiscard]] double pairsRatio() const { return static_cast<double>(penta_WW + penta_WD) / (penta_LD + penta_LL); }

    [[nodiscard]] double pointsRatio() const { return points() / sum() * 100; }

    [[nodiscard]] int totalPairs() const { return penta_WW + penta_WD + penta_WL + penta_DD + penta_LD + penta_LL; }

    int wins   = 0;
    int losses = 0;
    int draws  = 0;

    int penta_WW = 0;
    int penta_WD = 0;
    int penta_WL = 0;
    int penta_DD = 0;
    int penta_LD = 0;
    int penta_LL = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Stats, wins, losses, draws, penta_WW, penta_WD, penta_WL, penta_DD, penta_LD,
                                   penta_LL)

}  // namespace fastchess