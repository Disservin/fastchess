#pragma once

#include "chess/board.hpp"
#include "engines/engine_config.hpp"
#include "engines/uci_engine.hpp"
#include "options.hpp"

namespace fast_chess
{

struct MatchInfo
{
    std::vector<MoveData> moves;
    EngineConfiguration white_engine;
    EngineConfiguration black_engine;
    GameResult result = GameResult::NONE;
    std::string termination;
    std::string start_time;
    std::string end_time;
    std::string duration;
    std::string date;
    std::string fen;
    int round = 0;
    bool legal = true;
    bool needs_restart = false;
};

class Match
{

  public:
    Match() = default;

    Match(CMD::GameManagerOptions match_config, const EngineConfiguration &engine1_config,
          const EngineConfiguration &engine2_config, bool save_time_header);

    MatchInfo startMatch(int roundId, std::string openingFen);

  private:
    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const Score mate_score_ = 100'000;

    void initializeEngine(UciEngine &engine, const EngineConfiguration &config, Turn turn);

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
