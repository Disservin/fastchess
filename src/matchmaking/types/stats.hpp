#pragma once

#include <helper.hpp>

namespace fast_chess {

struct Stats {
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

    /// @brief invert the stats
    /// @return
    [[nodiscard]] Stats operator~() const {
        Stats stats = *this;
        std::swap(stats.wins, stats.losses);
        std::swap(stats.penta_WW, stats.penta_LL);
        std::swap(stats.penta_WD, stats.penta_LD);
        return stats;
    }

    [[nodiscard]] bool operator==(const Stats &rhs) const {
        return wins == rhs.wins && losses == rhs.losses && draws == rhs.draws &&
               penta_WW == rhs.penta_WW && penta_WD == rhs.penta_WD && penta_WL == rhs.penta_WL &&
               penta_DD == rhs.penta_DD && penta_LD == rhs.penta_LD && penta_LL == rhs.penta_LL;
    }

    [[nodiscard]] bool operator!=(const Stats &rhs) const { return !(*this == rhs); }

    [[nodiscard]] int sum() const { return wins + losses + draws; }

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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Stats, wins, losses, draws, penta_WW, penta_WD,
                                                penta_WL, penta_DD, penta_LD, penta_LL)

}  // namespace fast_chess