#pragma once

#include "engine_config.hpp"
#include "third_party/chess.hpp"
#include "third_party/communication.hpp"

namespace fast_chess {

enum class Turn { FIRST, SECOND };

constexpr Turn operator~(Turn t) {
    return Turn(static_cast<int>(t) ^ static_cast<int>(Turn::SECOND));
}

class UciEngine : public Communication::Process {
   public:
    UciEngine() = default;
    ~UciEngine() { sendQuit(); }

    std::vector<std::string> readUci();

    void sendUciNewGame();
    void sendUci();
    void sendQuit();

    bool isResponsive(int64_t threshold = ping_time_);

    void loadConfig(const EngineConfiguration &config);
    EngineConfiguration getConfig() const;

    std::string buildGoInput(Chess::Color stm, const TimeControl &tc,
                             const TimeControl &tc_2) const;

    void restartEngine();
    void startEngine(const std::string &cmd);

    std::vector<std::string> readEngine(std::string_view last_word,
                                        int64_t timeoutThreshold = 1000);
    void writeEngine(const std::string &input);

    static const int64_t ping_time_ = 60000;

   private:
    void sendSetoption(const std::string &name, const std::string &value);

    EngineConfiguration config_;
};
}  // namespace fast_chess
