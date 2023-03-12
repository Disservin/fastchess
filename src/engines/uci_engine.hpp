#pragma once

#include "../chess/types.hpp"
#include "engine_config.hpp"
#include "engineprocess.hpp"

namespace fast_chess
{

enum class Turn
{
    FIRST,
    SECOND
};

constexpr Turn operator~(Turn t)
{
    return Turn(static_cast<int>(t) ^ static_cast<int>(Turn::SECOND));
}

class UciEngine : public EngineProcess
{

  private:
    static const int64_t PING_TIME = 60000;
    EngineConfiguration config;

  public:
    UciEngine() = default;

    UciEngine(const std::string &command)
    {
        initProcess(command);
    }

    UciEngine(const EngineConfiguration &config)
    {
        setConfig(config);
    }

    ~UciEngine()
    {
        sendQuit();
    }

    EngineConfiguration getConfig() const;

    std::string checkErrors(int id = -1);

    bool isResponsive(int64_t threshold = PING_TIME);

    void sendUciNewGame();

    void sendUci();

    std::vector<std::string> readUci();

    std::string buildGoInput(Color stm, const TimeControl &tc, const TimeControl &tc_2) const;

    void loadConfig(const EngineConfiguration &config);

    void sendQuit();

    void sendSetoption(const std::string &name, const std::string &value);

    void sendGo(const std::string &limit);

    void setConfig(const EngineConfiguration &rhs);

    void restartEngine();

    void startEngine();

    void startEngine(const std::string &cmd);

    Turn turn = Turn::FIRST;
};
} // namespace fast_chess
