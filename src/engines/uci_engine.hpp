#pragma once

#include <chess.hpp>

#ifdef _WIN64
#include <process/process_win.hpp>
#else
#include <process/process_posix.hpp>
#endif

#include <types/engine_config.hpp>

namespace fast_chess {

class UciEngine : Process {
   public:
    explicit UciEngine(const EngineConfiguration &config) {
        loadConfig(config);
        startEngine();
    }

    ~UciEngine() override { sendQuit(); }

    void refreshUci();

    /// @brief Just writes "uci" to the engine
    void sendUci();

    /// @brief Reads until "uciok" is found, uses the default ping_time_ as the timeout thresholdd.
    /// @return
    [[nodiscard]] bool readUci();

    /// @brief Sends "ucinewgame" to the engine and waits for a response. Also uses the ping_time_
    /// as the timeout thresholdd.
    [[nodiscard]] bool sendUciNewGame();

    /// @brief Sends "quit" to the engine.
    void sendQuit();

    /// @brief Sends "isready" to the engine and waits for a response.
    /// @param threshold
    /// @return
    [[nodiscard]] bool isResponsive(std::chrono::milliseconds threshold = ping_time_);

    [[nodiscard]] EngineConfiguration getConfig() const;

    /// @brief Creates a new process and starts the engine.
    void startEngine();

    /// @brief Waits for the engine to output the last_word or until the threshold_ms is reached.
    /// May throw if the read fails.
    /// @param last_word
    /// @param threshold 0 means no timeout
    /// @return
    Process::Status readEngine(std::string_view last_word,
                               std::chrono::milliseconds threshold = ping_time_);

    /// @brief Writes the input to the engine. May throw if the write fails.
    /// @param input
    void writeEngine(const std::string &input);

    void setCpus(const std::vector<int> &cpus) { setAffinity(cpus); }

    /// @brief Get the bestmove from the last output.
    /// @return
    [[nodiscard]] std::string bestmove() const;

    /// @brief Get the last info from the last output.
    /// @return
    [[nodiscard]] std::vector<std::string> lastInfo() const;

    /// @brief Get the last score type from the last output. cp or mate.
    /// @return
    [[nodiscard]] std::string lastScoreType() const;

    /// @brief Get the last score from the last output. Becareful, mate scores are not converted. So
    /// the score might 1, while it's actually mate 1. Always check lastScoreType() first.
    /// @return
    [[nodiscard]] int lastScore() const;

    [[nodiscard]] const std::vector<std::string> &output() const;

    /// @brief TODO: expose this to the user
    static constexpr std::chrono::milliseconds ping_time_ = std::chrono::milliseconds(60000);

   private:
    void loadConfig(const EngineConfiguration &config);
    void sendSetoption(const std::string &name, const std::string &value);

    EngineConfiguration config_;

    std::vector<std::string> output_;
};
}  // namespace fast_chess
