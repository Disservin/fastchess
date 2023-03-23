#pragma once

#include "chess/types.hpp"
#include "engine_config.hpp"
#include "engineprocess.hpp"

namespace fast_chess {

enum class Turn { FIRST, SECOND };

constexpr Turn operator~(Turn t) {
    return Turn(static_cast<int>(t) ^ static_cast<int>(Turn::SECOND));
}

class UciEngine : public EngineProcess {
   public:
    UciEngine() = default;

    ~UciEngine() { sendQuit(); }

    EngineConfiguration getConfig() const;

    /// @brief
    /// @param id
    /// @return empty string if there are no errors
    std::string checkErrors(int id = -1);

    bool isResponsive(int64_t threshold = ping_time_);

    void sendUciNewGame();

    void sendUci();

    std::vector<std::string> readUci();

    void loadConfig(const EngineConfiguration &config);

    void sendQuit();

    std::string buildGoInput(Color stm, const TimeControl &tc, const TimeControl &tc_2) const;

    void restartEngine();

    void startEngine(const std::string &cmd);

    static const int64_t ping_time_ = 60000;

   private:
    EngineConfiguration config_;

    void sendSetoption(const std::string &name, const std::string &value);
};
}  // namespace fast_chess
