#pragma once

#include <string>

#include "../../options.hpp"
#include "../../sprt.hpp"
#include "../types/match_data.hpp"
#include "../types/stats.hpp"


namespace fast_chess {

class Tournament;  // forward declaration

class Output {
   public:
    Output() = default;
    virtual ~Output() = default;

    virtual void printInterval(const Stats& stats, const std::string& first,
                               const std::string& second, int total) = 0;

    virtual void printElo() = 0;

    virtual void startGame() = 0;

    virtual void endGame() = 0;

   protected:
    // SPRT sprt_;
};

// class Cutechess : public Output {
//    public:
//     Cutechess(const SPRT& sprt) { sprt_ = sprt; };

//     void printElo(Tournament& tournament, std::string first, std::string second) override;

//     void startMatch(const EngineConfiguration& engine1_config,
//                     const EngineConfiguration& engine2_config, int round_id,
//                     int total_count) override;

//     void endMatch(Tournament& tournament, const MatchData& match_data, int, int round_id)
//     override;
// };

}  // namespace fast_chess