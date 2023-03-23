#pragma once
#include "engines/engine_config.hpp"

namespace fast_chess {
struct Stats {
    int wins = 0;
    int losses = 0;
    int draws = 0;

    int penta_WW = 0;
    int penta_WD = 0;
    int penta_WL = 0;
    int penta_DD = 0;
    int penta_LD = 0;
    int penta_LL = 0;

    Stats &operator+=(const Stats &rhs) {
        this->wins += rhs.wins;
        this->losses += rhs.losses;
        this->draws += rhs.draws;

        this->penta_WW += rhs.penta_WW;
        this->penta_WD += rhs.penta_WD;
        this->penta_WL += rhs.penta_WL;
        this->penta_DD += rhs.penta_DD;
        this->penta_LD += rhs.penta_LD;
        this->penta_LL += rhs.penta_LL;
        return *this;
    }

    int sum() const { return wins + losses + draws; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Stats, wins, losses, draws, penta_WW, penta_WD,
                                                penta_WL, penta_DD, penta_LD, penta_LL)

constexpr Stats operator~(const Stats &rhs) {
    Stats stats;
    stats.wins = rhs.losses;
    stats.losses = rhs.wins;
    stats.draws = rhs.draws;

    stats.penta_WW = rhs.penta_LL;
    stats.penta_WD = rhs.penta_LD;
    stats.penta_WL = rhs.penta_WL;
    stats.penta_DD = rhs.penta_DD;
    stats.penta_LD = rhs.penta_WD;
    stats.penta_LL = rhs.penta_WW;

    return stats;
}
}  // namespace fast_chess
