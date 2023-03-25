#pragma once

#include <string>

#include "matchmaking/match_data.hpp"
#include "options.hpp"
#include "sprt.hpp"

namespace fast_chess {

class Tournament;  // forward declaration

class Output {
   public:
    Output() = default;
    virtual ~Output() = default;

    virtual void printElo(Tournament& tournament, std::string first, std::string second) = 0;

    virtual void startMatch(const EngineConfiguration& engine1_config,
                            const EngineConfiguration& engine2_config, int round_id,
                            int total_count) = 0;

    virtual void endMatch(Tournament& tournament, const MatchData& match_data, int game_id,
                          int round_id) = 0;

   protected:
    SPRT sprt_;
};

class Cutechess : public Output {
   public:
    Cutechess(const SPRT& sprt) { sprt_ = sprt; };

    void printElo(Tournament& tournament, std::string first, std::string second) override;

    void startMatch(const EngineConfiguration& engine1_config,
                    const EngineConfiguration& engine2_config, int round_id,
                    int total_count) override;

    void endMatch(Tournament& tournament, const MatchData& match_data, int, int round_id) override;
};

class Fastchess : public Output {
   public:
    Fastchess(const SPRT& sprt) { sprt_ = sprt; };

    void printElo(Tournament& tournament, std::string first, std::string second) override;

    void startMatch(const EngineConfiguration&, const EngineConfiguration&, int, int) override;

    void endMatch(Tournament& tournament, const MatchData& match_data, int game_id,
                  int round_id) override;
};

}  // namespace fast_chess
