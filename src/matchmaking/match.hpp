#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "chess/board.hpp"
#include "options.hpp"
#include "participant.hpp"

namespace fast_chess
{

struct MoveData
{
    std::string move;
    std::string score_string;
    int64_t elapsed_millis = 0;
    uint64_t nodes = 0;
    int seldepth = 0;
    int depth = 0;
    int score = 0;

    MoveData() = default;
    MoveData(std::string _move, std::string _score_string, int64_t _elapsed_millis, int _depth,
             int _seldepth, int _score, int _nodes)
        : move(_move), score_string(std::move(_score_string)), elapsed_millis(_elapsed_millis),
          depth(_depth), seldepth(_seldepth), score(_score), nodes(_nodes)
    {
    }
};

struct MatchData
{
    std::vector<MoveData> moves;
    std::pair<PlayerInfo, PlayerInfo> players;
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
    Match(CMD::GameManagerOptions game_config, const EngineConfiguration &engine1_config,
          const EngineConfiguration &engine2_config);

    void playMatch(const std::string &openingFen);

    MatchData getMatchData() const;

  private:
    /// @brief
    /// @param engine
    /// @param input
    /// @return true if tell was succesful
    bool tellEngine(UciEngine &engine, const std::string &input);

    bool hasErrors(UciEngine &engine);
    bool isResponsive(UciEngine &engine);

    MoveData parseEngineOutput(const std::vector<std::string> &output, const std::string &move,
                               int64_t measured_time);

    /// @brief
    /// @param player
    /// @param position_input
    /// @param time_left_us
    /// @param time_left_them
    /// @return false if move was illegal
    bool playNextMove(Participant &player, std::string &position_input, TimeControl &time_left_us,
                      const TimeControl &time_left_them);

    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    CMD::GameManagerOptions game_config_;

    Participant player_1_;
    Participant player_2_;

    Board board_;

    MatchData match_data_;
};

} // namespace fast_chess