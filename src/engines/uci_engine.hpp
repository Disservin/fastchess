#pragma once

#include "../third_party/chess.hpp"
#include "../third_party/communication.hpp"
#include "engine_config.hpp"

namespace fast_chess {

class UciEngine : public Communication::Process {
   public:
    UciEngine() = default;
    ~UciEngine() { sendQuit(); }

    void sendUci();
    [[nodiscard]] bool readUci();

    [[nodiscard]] bool sendUciNewGame();
    void sendQuit();

    [[nodiscard]] bool isResponsive(int64_t threshold = ping_time_);

    void loadConfig(const EngineConfiguration &config);
    [[nodiscard]] EngineConfiguration getConfig() const;

    [[nodiscard]] std::string buildPositionInput(const std::vector<std::string> &moves,
                                                 const std::string &fen) const;
    [[nodiscard]] std::string buildGoInput(Chess::Color stm, const TimeControl &tc,
                                           const TimeControl &tc_2) const;

    void restartEngine();
    void startEngine(const std::string &cmd);

    std::vector<std::string> readEngine(std::string_view last_word,
                                        int64_t timeoutThreshold = 1000);
    void writeEngine(const std::string &input);

    [[nodiscard]] std::string bestmove() const;
    [[nodiscard]] std::vector<std::string> lastInfo() const;
    [[nodiscard]] std::string lastScoreType() const;
    [[nodiscard]] int lastScore() const;

    [[nodiscard]] std::vector<std::string> output() const;

    static const int64_t ping_time_ = 60000;

   private:
    void sendSetoption(const std::string &name, const std::string &value);

    std::vector<std::string> output_;

    EngineConfiguration config_;
};
}  // namespace fast_chess
