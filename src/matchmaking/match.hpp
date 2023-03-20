#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "chess/board.hpp"
#include "matchmaking/match_data.hpp"
#include "matchmaking/participant.hpp"
#include "options.hpp"

namespace fast_chess
{

struct DrawAdjTracker
{
    Score draw_score = 0;
    int move_count = 0;

    DrawAdjTracker(Score draw_score, int move_count)
    {
        this->draw_score = draw_score;
        this->move_count = move_count;
    }
};

struct ResignAdjTracker
{
    int move_count = 0;
    Score resign_score = 0;

    ResignAdjTracker(Score resign_score, int move_count)
    {
        this->resign_score = resign_score;
        this->move_count = move_count;
    }
};

class Match
{
  public:
    Match(CMD::GameManagerOptions game_config, const EngineConfiguration &engine1_config,
          const EngineConfiguration &engine2_config);

    /// @brief plays a match between the previously loaded engines
    /// @param openingFen
    void playMatch(const std::string &openingFen);

    MatchData getMatchData();

  private:
    void updateTrackers(const Score moveScore, const int move_number);

    GameResult checkAdj(const Score score);

    /// @brief
    /// @param player
    /// @param input
    /// @return true if tell was succesful
    bool tellEngine(Participant &player, const std::string &input);

    /// @brief check if the engine encountered any lower level errors
    /// @param player
    /// @return
    bool hasErrors(Participant &player);

    bool isResponsive(Participant &player);

    /// @brief Extracts information from the engines reported info string
    /// @param output
    /// @param move
    /// @param measured_time
    /// @return
    MoveData parseEngineOutput(const std::vector<std::string> &output, const std::string &move,
                               int64_t measured_time);

    /// @brief Plays the next move and checks for game over and legalitly
    /// @param player
    /// @param position_input
    /// @param time_left_us
    /// @param time_left_them
    /// @return false if game has ended
    bool playNextMove(Participant &player, Participant &enemy, std::string &position_input,
                      TimeControl &time_left_us, const TimeControl &time_left_them);

    const std::string startpos_ = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const Score mate_score_ = 100'000;

    CMD::GameManagerOptions game_config_;

    ResignAdjTracker resignTracker_;
    DrawAdjTracker drawTracker_;

    Participant player_1_;
    Participant player_2_;

    Board board_;

    MatchData match_data_;
};

} // namespace fast_chess