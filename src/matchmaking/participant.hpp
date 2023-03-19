#pragma once

#include "engines/uci_engine.hpp"

namespace fast_chess
{

struct PlayerInfo
{
    std::string termination;
    std::string name;
    std::string result;
    GameResult score = GameResult::NONE;
    Color color = NO_COLOR;
};

class Participant
{
  public:
    Participant() = default;
    explicit Participant(const EngineConfiguration &config);

    PlayerInfo &getInfo()
    {
        return info_;
    }

    UciEngine engine_;

  private:
    PlayerInfo info_;
};

} // namespace fast_chess