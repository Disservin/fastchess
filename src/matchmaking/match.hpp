#pragma once

#include <optional>

#include "chess/board.hpp"
#include "engines/engine_config.hpp"
#include "engines/uci_engine.hpp"
#include "options.hpp"

namespace fast_chess
{

class Match
{

  public:
    Match() = default;

    Match(CMD::GameManagerOptions match_config, const EngineConfiguration &engine1_config,
          const EngineConfiguration &engine2_config, bool save_time_header);

    MatchInfo startMatch(int roundId, std::string openingFen);

    template <typename T>
    std::optional<T> findElement(const std::vector<std::string> &haystack, std::string_view needle)
    {
        auto position = std::find(haystack.begin(), haystack.end(), needle);
        auto index = position - haystack.begin();
        if (position == haystack.end())
            return std::nullopt;
        if constexpr (std::is_same_v<T, int>)
            return std::stoi(haystack[index + 1]);
        else if constexpr (std::is_same_v<T, float>)
            return std::stof(haystack[index + 1]);
        else if constexpr (std::is_same_v<T, uint64_t>)
            return std::stoull(haystack[index + 1]);
        else
            return haystack[index + 1];
    }

  private:
    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const Score mate_score_ = 100'000;

    MoveData parseEngineOutput(const std::vector<std::string> &output, const std::string &move,
                               int64_t measuredTime);

    void updateTrackers(const Score moveScore, const int move_number);

    GameResult checkAdj(const Score score, const Color lastSideThatMoved);

    bool checkEngineStatus(UciEngine &engine);

    bool playNextMove(UciEngine &engine, std::string &positionInput, TimeControl &timeLeftUs,
                      const TimeControl &timeLeftThem);

    CMD::GameManagerOptions match_config_;
    ResignAdjTracker resignTracker_;
    DrawAdjTracker drawTracker_;

    UciEngine engine1_;
    UciEngine engine2_;

    MatchInfo mi_;

    Board board_;

    int roundId_ = 0;

    bool save_time_header_ = false;
};

} // namespace fast_chess
